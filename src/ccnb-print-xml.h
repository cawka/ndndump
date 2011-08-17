/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#ifndef _CCNB_DECODER_H_
#define _CCNB_DECODER_H_

#include "ccnx-decoding-helper.h"

extern "C"
{
#include <ccn/coding.h>
} // extern "C"

/* formatting_flags */
#define FORCE_BINARY   (1 << 0)
#define PREFER_HEX     (1 << 1)
#define VERBOSE_DECODE (1 << 2)

#define CCN_NO_SCHEMA INT_MIN
#define CCN_UNKNOWN_SCHEMA (INT_MIN+1)

class CcnbXmlPrinter : public GJVoidVisitor
{
public:
  CcnbXmlPrinter (int formatting_flags, const ccn_dict *dtags);
  ~CcnbXmlPrinter ();

  size_t
  DecodeAndPrint (const char *p, size_t n);

public:
  virtual void visit (Blob& n, boost::any param);
  virtual void visit (Udata&n, boost::any param);
  virtual void visit (Attr& n, boost::any param);
  virtual void visit (Tag&  n, boost::any param);
  virtual void visit (Dtag& n, boost::any param);
  virtual void visit (Dattr&n, boost::any param);
  virtual void visit (Ext&  n, boost::any param);

private:
  void ProcessTag (BaseTag &n, boost::any param);
  
private:
  const ccn_dict_entry *m_tagdict;
  int m_tagdict_count;
  int m_formatting_flags;
};

class DecoderException {};

#endif // _CCNB_DECODER_H_
