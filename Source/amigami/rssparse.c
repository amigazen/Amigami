/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rssparse.c - RSS/Atom XML parser (adapted from AWebRssAPL)
 */

#include "rss.h"
#include <exec/memory.h>
#include <exec/types.h>
#include <string.h>
#include <ctype.h>
#include <proto/exec.h>
#include <proto/utility.h>

#include "utf8fold.h"
#include "amlog.h"

/* Allocate and initialize parser */
void InitRssParser(struct RssParser *parser)
{  struct FeedChannel *channel;
   
   if(!parser) return;
   
   parser->buffer = NULL;
   parser->bufsize = 0;
   parser->buflen = 0;
   parser->current = NULL;
   parser->end = NULL;
   parser->currenttag = NULL;
   parser->currenttaglen = 0;
   parser->currentdata = NULL;
   parser->currentdatalen = 0;
   parser->initem = FALSE;
   parser->incontent = FALSE;
   parser->detectedtype = FEED_UNKNOWN;
   
   channel = (struct FeedChannel *)AllocVec(sizeof(struct FeedChannel), MEMF_CLEAR);
   if(channel)
   {  channel->items = NULL;
      channel->lastitem = NULL;
      channel->itemcount = 0;
      channel->feedtype = FEED_UNKNOWN;
   }
   parser->channel = channel;
   parser->currentitem = NULL;
}

/* Helper: Find end of tag name (stop before attrs / whitespace) */
static UBYTE *FindTagEnd(UBYTE *p, UBYTE *end)
{  while(p < end && *p != '>' && *p != ' ' && *p != '\t' &&
         *p != '/' && *p != '\n' && *p != '\r' && *p != '=')
   {  p++;
   }
   return p;
}

/* Helper: Find closing tag. Returns pointer to the '<' of "</name>". */
static UBYTE *FindClosingTag(UBYTE *p, UBYTE *end, UBYTE *tagname, long tagnamelen)
{  UBYTE *tagstart;
   UBYTE *tagend;
   UBYTE *lt;
   
   while(p < end)
   {  if(*p == '<')
      {  lt = p;
         if(p + 1 < end && p[1] == '/')
         {  tagstart = p + 2;
            tagend = FindTagEnd(tagstart, end);
            if(tagend - tagstart == tagnamelen)
            {  if(strnicmp(tagstart, tagname, tagnamelen) == 0)
               {  return lt;
               }
            }
            /* Skip rest of this closing tag */
            p = tagend;
            while(p < end && *p != '>') p++;
            if(p < end) p++;
            continue;
         }
         /* Skip non-closing tags quickly (HTML inside description). */
         p++;
         while(p < end && *p != '>') p++;
         if(p < end) p++;
         continue;
      }
      p++;
   }
   return NULL;
}

/* Helper: Extract text content from tag (unwraps CDATA). */
static UBYTE *ExtractText(UBYTE *start, UBYTE *end, long *len)
{  UBYTE *textstart;
   UBYTE *textend;
   UBYTE *p;
   BOOL isCdata;
   
   textstart = start;
   while(textstart < end && (*textstart == ' ' || *textstart == '\t' || 
         *textstart == '\n' || *textstart == '\r'))
   {  textstart++;
   }
   
   textend = end;
   while(textend > textstart && (textend[-1] == ' ' || textend[-1] == '\t' || 
         textend[-1] == '\n' || textend[-1] == '\r'))
   {  textend--;
   }

   /*
    * CDATA: <![CDATA[ ... ]]>  (also tolerate missing '<' from older bad trim
    * leaving ![CDATA[ ... ]]).
    */
   isCdata = FALSE;
   if(textstart + 9 <= textend && textstart[0] == '<' &&
      textstart[1] == '!' && textstart[2] == '[' &&
      (textstart[3] == 'C' || textstart[3] == 'c') &&
      (textstart[4] == 'D' || textstart[4] == 'd') &&
      (textstart[5] == 'A' || textstart[5] == 'a') &&
      (textstart[6] == 'T' || textstart[6] == 't') &&
      (textstart[7] == 'A' || textstart[7] == 'a') &&
      textstart[8] == '[')
   {  textstart += 9;
      isCdata = TRUE;
   }
   else if(textstart + 8 <= textend && textstart[0] == '!' &&
      textstart[1] == '[' &&
      (textstart[2] == 'C' || textstart[2] == 'c') &&
      (textstart[3] == 'D' || textstart[3] == 'd') &&
      (textstart[4] == 'A' || textstart[4] == 'a') &&
      (textstart[5] == 'T' || textstart[5] == 't') &&
      (textstart[6] == 'A' || textstart[6] == 'a') &&
      textstart[7] == '[')
   {  textstart += 8;
      isCdata = TRUE;
   }

   if(isCdata)
   {  p = textstart;
      while(p + 2 < textend)
      {  if(p[0] == ']' && p[1] == ']' && p[2] == '>')
         {  textend = p;
            break;
         }
         p++;
      }
      /* Trailing ]]> without full three-char end */
      if(textend > textstart && textend[-1] == ']')
      {  while(textend > textstart && textend[-1] == ']')
            textend--;
      }
   }
   
   /* Decode HTML entities */
   p = textstart;
   while(p < textend)
   {  if(*p == '&')
      {  if(p + 3 < textend && strnicmp(p, "&lt;", 4) == 0)
         {  *p = '<';
            memmove(p + 1, p + 4, textend - p - 4);
            textend -= 3;
         }
         else if(p + 3 < textend && strnicmp(p, "&gt;", 4) == 0)
         {  *p = '>';
            memmove(p + 1, p + 4, textend - p - 4);
            textend -= 3;
         }
         else if(p + 4 < textend && strnicmp(p, "&amp;", 5) == 0)
         {  *p = '&';
            memmove(p + 1, p + 5, textend - p - 5);
            textend -= 4;
         }
         else if(p + 5 < textend && strnicmp(p, "&quot;", 6) == 0)
         {  *p = '"';
            memmove(p + 1, p + 6, textend - p - 6);
            textend -= 5;
         }
         else if(p + 5 < textend && strnicmp(p, "&apos;", 6) == 0)
         {  *p = '\'';
            memmove(p + 1, p + 6, textend - p - 6);
            textend -= 5;
         }
      }
      p++;
   }
   
   if(len) *len = textend - textstart;
   return textstart;
}

/* Unwrap a single simple <tag>...</tag> wrapper (Atom <author><name>...).</ */
static void UnwrapSimpleElement(UBYTE **start, UBYTE **end)
{  UBYTE *s;
   UBYTE *e;
   UBYTE *gt;
   UBYTE *close;

   s = *start;
   e = *end;
   if(s == NULL || e == NULL || s >= e) return;
   while(s < e && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
   while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;
   if(s >= e || *s != '<') return;
   gt = s + 1;
   while(gt < e && *gt != '>') gt++;
   if(gt >= e) return;
   /* Prefer innermost text: find first '>' then last '<' before end */
   close = e;
   while(close > s && *close != '<') close--;
   if(close <= s || close[1] != '/') return;
   *start = gt + 1;
   *end = close;
   while(*start < *end && (**start == ' ' || **start == '\t' ||
      **start == '\n' || **start == '\r'))
      (*start)++;
   while(*end > *start && ((*end)[-1] == ' ' || (*end)[-1] == '\t' ||
      (*end)[-1] == '\n' || (*end)[-1] == '\r'))
      (*end)--;
}

/* Duplicate string as Amiga Latin-1 (UTF-8 folded at parse time). */
static UBYTE *DupString(UBYTE *start, long len)
{  if(!start || len <= 0) return NULL;
   return Utf8DupLatin1(start, len);
}

/*
 * Only leaf text tags should run FindClosingTag + ProcessContent.
 * Structural tags (rss/channel/item/entry/feed/...) must be walked tag-by-tag
 * or nested <title>/<link> inside <item> are skipped (log: items=0).
 */
static BOOL IsLeafContentTag(UBYTE *name, long namelen)
{
   if(name == NULL || namelen <= 0) return FALSE;
   if(namelen == 5 && strnicmp(name, "title", 5) == 0) return TRUE;
   if(namelen == 4 && strnicmp(name, "link", 4) == 0) return TRUE;
   if(namelen == 11 && strnicmp(name, "description", 11) == 0) return TRUE;
   if(namelen == 7 && strnicmp(name, "content", 7) == 0) return TRUE;
   if(namelen == 15 && strnicmp(name, "content:encoded", 15) == 0) return TRUE;
   if(namelen == 7 && strnicmp(name, "summary", 7) == 0) return TRUE;
   if(namelen == 7 && strnicmp(name, "pubDate", 7) == 0) return TRUE;
   if(namelen == 7 && strnicmp(name, "updated", 7) == 0) return TRUE;
   if(namelen == 8 && strnicmp(name, "published", 8) == 0) return TRUE;
   if(namelen == 6 && strnicmp(name, "author", 6) == 0) return TRUE;
   if(namelen == 4 && strnicmp(name, "name", 4) == 0) return TRUE; /* atom author */
   if(namelen == 4 && strnicmp(name, "guid", 4) == 0) return TRUE;
   if(namelen == 2 && strnicmp(name, "id", 2) == 0) return TRUE;
   if(namelen == 8 && strnicmp(name, "language", 8) == 0) return TRUE;
   if(namelen == 9 && strnicmp(name, "copyright", 9) == 0) return TRUE;
   if(namelen == 8 && strnicmp(name, "subtitle", 8) == 0) return TRUE;
   if(namelen == 8 && strnicmp(name, "category", 8) == 0) return TRUE;
   if(namelen == 8 && strnicmp(name, "dc:title", 8) == 0) return TRUE;
   if(namelen == 10 && strnicmp(name, "dc:creator", 10) == 0) return TRUE;
   if(namelen == 7 && strnicmp(name, "dc:date", 7) == 0) return TRUE;
   return FALSE;
}

/* Process a tag */
static void ProcessTag(struct RssParser *parser, UBYTE *tagstart, UBYTE *tagend, BOOL isclosing)
{  UBYTE *tagname;
   long tagnamelen;
   struct FeedItem *item;
   struct FeedChannel *channel;
   
   if(!parser || !tagstart || !tagend) return;
   
   channel = parser->channel;
   if(!channel) return;
   
   tagname = tagstart;
   tagnamelen = tagend - tagstart;
   
   if(isclosing)
   {  /* Closing tag */
      if(parser->initem && parser->currentitem)
      {  if(parser->currenttag && parser->currenttaglen == tagnamelen)
         {  if(strnicmp(parser->currenttag, tagname, tagnamelen) == 0)
            {  parser->incontent = FALSE;
               parser->currenttag = NULL;
               parser->currenttaglen = 0;
            }
         }
      }
      
      if(tagnamelen == 4 && strnicmp(tagname, "item", 4) == 0)
      {  parser->initem = FALSE;
         parser->currentitem = NULL;
      }
      else if(tagnamelen == 5 && strnicmp(tagname, "entry", 5) == 0)
      {  parser->initem = FALSE;
         parser->currentitem = NULL;
      }
      return;
   }
   
   /* Opening tag - detect feed type */
   if(parser->detectedtype == FEED_UNKNOWN)
   {  if(tagnamelen == 3 && strnicmp(tagname, "rss", 3) == 0)
      {  parser->detectedtype = FEED_RSS;
         channel->feedtype = FEED_RSS;
      }
      else if(tagnamelen == 4 && strnicmp(tagname, "feed", 4) == 0)
      {  parser->detectedtype = FEED_ATOM;
         channel->feedtype = FEED_ATOM;
      }
   }
   
   /* RSS channel tags */
   if(!parser->initem)
   {  if(tagnamelen == 5 && strnicmp(tagname, "title", 5) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 5;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 4 && strnicmp(tagname, "link", 4) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 4;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 11 && strnicmp(tagname, "description", 11) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 11;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 8 && strnicmp(tagname, "language", 8) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 8;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 9 && strnicmp(tagname, "copyright", 9) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 9;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 4 && strnicmp(tagname, "item", 4) == 0)
      {  parser->initem = TRUE;
         item = (struct FeedItem *)AllocVec(sizeof(struct FeedItem), MEMF_CLEAR);
         if(item)
         {  if(channel->lastitem)
            {  channel->lastitem->next = item;
            }
            else
            {  channel->items = item;
            }
            channel->lastitem = item;
            channel->itemcount++;
            parser->currentitem = item;
         }
      }
      else if(tagnamelen == 5 && strnicmp(tagname, "entry", 5) == 0)
      {  parser->initem = TRUE;
         item = (struct FeedItem *)AllocVec(sizeof(struct FeedItem), MEMF_CLEAR);
         if(item)
         {  if(channel->lastitem)
            {  channel->lastitem->next = item;
            }
            else
            {  channel->items = item;
            }
            channel->lastitem = item;
            channel->itemcount++;
            parser->currentitem = item;
         }
      }
   }
   else
   {  /* Item/entry tags */
      item = parser->currentitem;
      if(!item) return;
      
      if(tagnamelen == 5 && strnicmp(tagname, "title", 5) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 5;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 4 && strnicmp(tagname, "link", 4) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 4;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 11 && strnicmp(tagname, "description", 11) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 11;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 7 && strnicmp(tagname, "content", 7) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 7;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 15 && strnicmp(tagname, "content:encoded", 15) == 0)
      {  /* BBC / many RSS2 full-HTML bodies */
         parser->currenttag = tagname;
         parser->currenttaglen = 15;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 7 && strnicmp(tagname, "summary", 7) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 7;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 7 && strnicmp(tagname, "pubDate", 7) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 7;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 7 && strnicmp(tagname, "updated", 7) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 7;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 8 && strnicmp(tagname, "published", 8) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 8;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 6 && strnicmp(tagname, "author", 6) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 6;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 4 && strnicmp(tagname, "name", 4) == 0)
      {  /* Atom <author><name> */
         parser->currenttag = tagname;
         parser->currenttaglen = 4;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 10 && strnicmp(tagname, "dc:creator", 10) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 10;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 4 && strnicmp(tagname, "guid", 4) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 4;
         parser->incontent = TRUE;
      }
      else if(tagnamelen == 2 && strnicmp(tagname, "id", 2) == 0)
      {  parser->currenttag = tagname;
         parser->currenttaglen = 2;
         parser->incontent = TRUE;
      }
   }
}

/* Process text content */
static void ProcessContent(struct RssParser *parser, UBYTE *start, UBYTE *end)
{  UBYTE *text;
   long textlen;
   struct FeedItem *item;
   struct FeedChannel *channel;
   
   if(!parser || !start || !end || !parser->incontent) return;
   
   channel = parser->channel;
   if(!channel) return;
   
   text = ExtractText(start, end, &textlen);
   if(!text || textlen <= 0) return;

   /* Atom <author><name>...</name></author> etc. */
   {  UBYTE *t2;
      UBYTE *e2;

      t2 = text;
      e2 = text + textlen;
      UnwrapSimpleElement(&t2, &e2);
      UnwrapSimpleElement(&t2, &e2);
      if(e2 > t2)
      {  text = t2;
         textlen = e2 - t2;
      }
   }
   if(textlen <= 0) return;
   
   if(parser->initem && parser->currentitem)
   {  item = parser->currentitem;
      
      if(parser->currenttag)
      {  if(parser->currenttaglen == 5 && strnicmp(parser->currenttag, "title", 5) == 0)
         {  if(!item->title)
            {  item->title = DupString(text, textlen);
               item->titlelen = textlen;
            }
         }
         else if(parser->currenttaglen == 4 && strnicmp(parser->currenttag, "link", 4) == 0)
         {  if(!item->link)
            {  item->link = DupString(text, textlen);
               item->linklen = textlen;
            }
         }
         else if(parser->currenttaglen == 11 && strnicmp(parser->currenttag, "description", 11) == 0)
         {  if(!item->description)
            {  item->description = DupString(text, textlen);
               item->desclen = textlen;
            }
         }
         else if(parser->currenttaglen == 7 && strnicmp(parser->currenttag, "content", 7) == 0)
         {  /* Prefer full content over short description */
            if(item->description) FreeVec(item->description);
            item->description = DupString(text, textlen);
            item->desclen = textlen;
         }
         else if(parser->currenttaglen == 15 &&
            strnicmp(parser->currenttag, "content:encoded", 15) == 0)
         {  if(item->description) FreeVec(item->description);
            item->description = DupString(text, textlen);
            item->desclen = textlen;
         }
         else if(parser->currenttaglen == 7 && strnicmp(parser->currenttag, "summary", 7) == 0)
         {  if(!item->description)
            {  item->description = DupString(text, textlen);
               item->desclen = textlen;
            }
         }
         else if(parser->currenttaglen == 7 && strnicmp(parser->currenttag, "pubDate", 7) == 0)
         {  if(!item->pubdate)
            {  item->pubdate = DupString(text, textlen);
               item->pubdatelen = textlen;
            }
         }
         else if(parser->currenttaglen == 7 && strnicmp(parser->currenttag, "updated", 7) == 0)
         {  if(!item->pubdate)
            {  item->pubdate = DupString(text, textlen);
               item->pubdatelen = textlen;
            }
         }
         else if(parser->currenttaglen == 8 && strnicmp(parser->currenttag, "published", 8) == 0)
         {  if(!item->pubdate)
            {  item->pubdate = DupString(text, textlen);
               item->pubdatelen = textlen;
            }
         }
         else if(parser->currenttaglen == 6 && strnicmp(parser->currenttag, "author", 6) == 0)
         {  if(!item->author)
            {  item->author = DupString(text, textlen);
               item->authorlen = textlen;
            }
         }
         else if(parser->currenttaglen == 4 && strnicmp(parser->currenttag, "name", 4) == 0)
         {  if(!item->author)
            {  item->author = DupString(text, textlen);
               item->authorlen = textlen;
            }
         }
         else if(parser->currenttaglen == 10 && strnicmp(parser->currenttag, "dc:creator", 10) == 0)
         {  if(!item->author)
            {  item->author = DupString(text, textlen);
               item->authorlen = textlen;
            }
         }
         else if(parser->currenttaglen == 4 && strnicmp(parser->currenttag, "guid", 4) == 0)
         {  if(!item->guid)
            {  item->guid = DupString(text, textlen);
               item->guidlen = textlen;
            }
         }
         else if(parser->currenttaglen == 2 && strnicmp(parser->currenttag, "id", 2) == 0)
         {  if(!item->guid)
            {  item->guid = DupString(text, textlen);
               item->guidlen = textlen;
            }
         }
      }
   }
   else
   {  /* Channel/feed metadata */
      if(parser->currenttag)
      {  if(parser->currenttaglen == 5 && strnicmp(parser->currenttag, "title", 5) == 0)
         {  if(!channel->title)
            {  channel->title = DupString(text, textlen);
               channel->titlelen = textlen;
            }
         }
         else if(parser->currenttaglen == 4 && strnicmp(parser->currenttag, "link", 4) == 0)
         {  if(!channel->link)
            {  channel->link = DupString(text, textlen);
               channel->linklen = textlen;
            }
         }
         else if(parser->currenttaglen == 11 && strnicmp(parser->currenttag, "description", 11) == 0)
         {  if(!channel->description)
            {  channel->description = DupString(text, textlen);
               channel->desclen = textlen;
            }
         }
         else if(parser->currenttaglen == 8 && strnicmp(parser->currenttag, "language", 8) == 0)
         {  if(!channel->language)
            {  channel->language = DupString(text, textlen);
               channel->languagelen = textlen;
            }
         }
         else if(parser->currenttaglen == 9 && strnicmp(parser->currenttag, "copyright", 9) == 0)
         {  if(!channel->copyright)
            {  channel->copyright = DupString(text, textlen);
               channel->copyrightlen = textlen;
            }
         }
      }
   }
}

/* Extract attribute value from tag body: name="value" or name='value'. */
static UBYTE *ExtractAttr(UBYTE *attrs, UBYTE *end, STRPTR name, long *outLen)
{  long nlen;
   UBYTE *p;
   UBYTE quote;

   if(outLen) *outLen = 0;
   if(!attrs || !end || !name) return NULL;
   nlen = (long)strlen((char *)name);
   p = attrs;
   while(p + nlen < end)
   {  if(strnicmp(p, name, nlen) == 0 &&
         (p == attrs || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n'))
      {  p += nlen;
         while(p < end && (*p == ' ' || *p == '\t')) p++;
         if(p < end && *p == '=')
         {  p++;
            while(p < end && (*p == ' ' || *p == '\t')) p++;
            if(p < end && (*p == '"' || *p == '\''))
            {  quote = *p++;
               attrs = p;
               while(p < end && *p != quote) p++;
               if(outLen) *outLen = p - attrs;
               return attrs;
            }
         }
      }
      p++;
   }
   return NULL;
}

static void ApplyLinkHref(struct RssParser *parser, UBYTE *attrs, UBYTE *gt)
{  UBYTE *href;
   long hrefLen;
   struct FeedItem *item;
   struct FeedChannel *channel;

   if(!parser || !attrs || !gt) return;
   href = ExtractAttr(attrs, gt, (STRPTR)"href", &hrefLen);
   if(!href || hrefLen <= 0) return;

   if(parser->initem && parser->currentitem)
   {  item = parser->currentitem;
      if(!item->link)
      {  item->link = DupString(href, hrefLen);
         item->linklen = hrefLen;
      }
   }
   else
   {  channel = parser->channel;
      if(channel && !channel->link)
      {  channel->link = DupString(href, hrefLen);
         channel->linklen = hrefLen;
      }
   }
}

/* Parse a chunk of XML data */
void ParseRssChunk(struct RssParser *parser, UBYTE *data, long length)
{  UBYTE *p;
   UBYTE *end;
   UBYTE *tagstart;
   UBYTE *tagend;
   UBYTE *gt;
   UBYTE *contentstart;
   UBYTE *closingtag;
   UBYTE *cdataend;
   BOOL isclosing;
   BOOL selfclosing;
   UBYTE *newbuffer;
   long newlen;
   long namelen;
   
   if(!parser || !data || length <= 0) return;
   
   /* Append to buffer */
   newlen = parser->buflen + length;
   if(newlen > parser->bufsize)
   {  newbuffer = (UBYTE *)AllocVec(newlen + 4096, MEMF_CLEAR);
      if(newbuffer)
      {  if(parser->buffer)
         {  memcpy(newbuffer, parser->buffer, parser->buflen);
            FreeVec(parser->buffer);
         }
         parser->buffer = newbuffer;
         parser->bufsize = newlen + 4096;
      }
      else
      {  return;
      }
   }
   
   memcpy(parser->buffer + parser->buflen, data, length);
   parser->buflen += length;
   
   p = parser->buffer;
   end = parser->buffer + parser->buflen;
   
   while(p < end)
   {  if(*p != '<')
      {  p++;
         continue;
      }

      tagstart = p + 1;
      if(tagstart >= end) break;

      /* Skip XML declarations / comments / CDATA markers at top level */
      if(*tagstart == '?' || *tagstart == '!')
      {  if(tagstart + 8 < end && tagstart[1] == '[' &&
            (tagstart[2] == 'C' || tagstart[2] == 'c'))
         {  /* stray CDATA - skip to ]]> */
            p = tagstart;
            while(p + 2 < end && !(p[0]==']' && p[1]==']' && p[2]=='>'))
               p++;
            if(p + 2 < end) p += 3;
            else break;
            continue;
         }
         while(p < end && *p != '>') p++;
         if(p < end) p++;
         continue;
      }

      isclosing = FALSE;
      if(*tagstart == '/')
      {  isclosing = TRUE;
         tagstart++;
      }

      tagend = FindTagEnd(tagstart, end);
      if(tagend >= end) break;
      namelen = tagend - tagstart;

      /* Find end of start-tag '>' (attrs may follow the name) */
      gt = tagend;
      while(gt < end && *gt != '>') gt++;
      if(gt >= end) break;

      selfclosing = FALSE;
      {  UBYTE *check = gt - 1;
         while(check > tagstart && (*check == ' ' || *check == '\t'))
            check--;
         if(check >= tagstart && *check == '/')
            selfclosing = TRUE;
      }

      ProcessTag(parser, tagstart, tagend, isclosing);

      /* Atom / HTML5-style <link href="..."/> */
      if(!isclosing && namelen == 4 && strnicmp(tagstart, "link", 4) == 0)
      {  ApplyLinkHref(parser, tagend, gt);
      }

      /*
       * Only leaf tags extract text to their closing tag. Structural tags
       * (item/entry/channel/rss/feed) advance past '>' and keep scanning.
       */
      if(!isclosing && !selfclosing && IsLeafContentTag(tagstart, namelen))
      {  contentstart = gt + 1;
         while(contentstart < end && (*contentstart == ' ' ||
            *contentstart == '\t' || *contentstart == '\n' ||
            *contentstart == '\r'))
         {  contentstart++;
         }

         /* Prefer CDATA span when present (may contain raw HTML '</...>') */
         if(contentstart + 9 <= end &&
            contentstart[0] == '<' && contentstart[1] == '!' &&
            contentstart[2] == '[')
         {  /* Prefer CDATA span when present (may contain raw HTML '</...>') */
            cdataend = contentstart + 9;
            while(cdataend + 2 < end &&
               !(cdataend[0]==']' && cdataend[1]==']' && cdataend[2]=='>'))
            {  cdataend++;
            }
            if(cdataend + 2 < end)
            {  ProcessContent(parser, contentstart, cdataend);
               p = cdataend + 3;
               while(p < end && (*p == ' ' || *p == '\t' ||
                  *p == '\n' || *p == '\r'))
                  p++;
               if(p < end && *p == '<' && p + 1 < end && p[1] == '/')
               {  while(p < end && *p != '>') p++;
                  if(p < end) p++;
               }
               parser->incontent = FALSE;
               parser->currenttag = NULL;
               parser->currenttaglen = 0;
               continue;
            }
         }

         closingtag = FindClosingTag(contentstart, end, tagstart, namelen);
         if(closingtag)
         {  ProcessContent(parser, contentstart, closingtag);
            p = closingtag;
            while(p < end && *p != '>') p++;
            if(p < end) p++;
            parser->incontent = FALSE;
            parser->currenttag = NULL;
            parser->currenttaglen = 0;
            continue;
         }
      }
      else if(selfclosing || isclosing)
      {  if(isclosing == FALSE || selfclosing)
         {  /* leaf self-close already handled; clear content state */
            ;
         }
         if(namelen > 0 && IsLeafContentTag(tagstart, namelen))
         {  parser->incontent = FALSE;
            parser->currenttag = NULL;
            parser->currenttaglen = 0;
         }
      }

      p = gt + 1;
   }
}

/* Cleanup parser */
void CleanupRssParser(struct RssParser *parser)
{  if(!parser) return;
   
   if(parser->buffer)
   {  FreeVec(parser->buffer);
      parser->buffer = NULL;
   }
   
   if(parser->channel)
   {  FreeFeedChannel(parser->channel);
      parser->channel = NULL;
   }
   
   parser->bufsize = 0;
   parser->buflen = 0;
}

struct FeedChannel *RssParserTakeChannel(struct RssParser *parser)
{  struct FeedChannel *ch;

   if(!parser) return NULL;
   ch = parser->channel;
   parser->channel = NULL;
   return ch;
}

struct FeedChannel *ParseRssBuffer(UBYTE *data, long length)
{  struct RssParser parser;
   struct FeedChannel *ch;
   struct FeedItem *it;
   long n;

   if(!data || length <= 0) return NULL;

   AmLog("Amigami: ParseRssBuffer len=%ld\n", (long)length);
   AmLogHex((STRPTR)"feed.head", data, (ULONG)length, 96);

   InitRssParser(&parser);
   if(!parser.channel)
   {  CleanupRssParser(&parser);
      return NULL;
   }

   ParseRssChunk(&parser, data, length);
   ch = RssParserTakeChannel(&parser);
   CleanupRssParser(&parser);

   if(ch && ch->itemcount <= 0)
   {     AmLog("Amigami: parse yielded 0 items (type=%ld) - fail\n",
         ch ? (long)ch->feedtype : -1L);
      FreeFeedChannel(ch);
      return NULL;
   }

   if(ch)
   {  AmLog("Amigami: parsed type=%ld items=%ld title=\"%s\"\n",
         (long)ch->feedtype, (long)ch->itemcount,
         ch->title ? (char *)ch->title : "(none)");
      AmLogStringCheck((STRPTR)"channel.title", (STRPTR)ch->title);
      n = 0;
      it = ch->items;
      while(it != NULL && n < 2)
      {  AmLog("Amigami:  item[%ld] title=\"%.50s\" date=\"%.24s\" desclen=%ld\n",
            n,
            it->title ? (char *)it->title : "(null)",
            it->pubdate ? (char *)it->pubdate : "",
            (long)it->desclen);
         AmLogStringCheck((STRPTR)"item.title", (STRPTR)it->title);
         it = it->next;
         n++;
      }
   }
   return ch;
}

