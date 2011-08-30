/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#ifndef _PRINT_HELPER_H_
#define _PRINT_HELPER_H_

#include <stddef.h>

struct ccn_dict_entry;

class PrintHelper
{
public:
  static const char *
  dict_name_from_number (int ndx, const struct ccn_dict_entry *dict, int n);
  
  static int
  is_text_encodable (const unsigned char *p, size_t start, size_t length);
  
  /* see ccn_uri_append_percentescaped */
  static void
  print_percent_escaped (const unsigned char *data, size_t size);

  static void
  print_hex_ascii_line (const unsigned char *payload, int len, int offset);

  static void
  print_payload (const unsigned char *payload, int len);

  static const char Base64[];
};

class UnknownDtag {};

#endif // _PRINT_HELPER_H_
