/*
 * bencode.h
 *
 * Copyright (c) 2022 Cisco Systems, Inc. All rights reserved.
 * License at https://github.com/cisco/mercury/blob/master/LICENSE
 */

#ifndef BENCODE_H
#define BENCODE_H

#include <stdint.h>
#include "datum.h"
#include "json_object.h"
#include "lex.h"

namespace bencoding {

    // Bencoding, following "BitTorrentSpecification - TheoryOrg.html"
    //
    // Bencoding is a way to specify and organize data in a terse
    // format. It supports the following types: byte strings, integers,
    // lists, and dictionaries.
    //

    // Byte strings are encoded as follows:
    // <string length encoded in base ten ASCII>:<string data>
    //
    //


    class byte_string {
        uint64_t len = 0;
        datum val;
        static constexpr uint64_t max_len = 256; // restricting the max len of byte string to 256 bytes

    public:

        byte_string(datum &d) {
            // loop over digits and compute value
            //
            while (d.is_not_empty()) {
                encoded<uint8_t> c(d);
                if (c.value() == ':') {
                    break;          // at end; not an error
                }
                if (c.value() < '0' || c.value() > '9') {
                    d.set_null();   // error; input is not a bint
                    break;
                }
                len *= 10;
                len += c.value() - '0';

                if (len > max_len) {
                    // Might be a bad packet.
                    d.set_null();
                    break;
                }
            }
            val.parse(d, len);
        }

        datum value() const { return val; }

        bool is_printable_ascii() const {
            datum tmp = val;
            while(tmp.is_readable()) {
                encoded<uint8_t> c(tmp);
                if (!c) {
                    return false;
                }
                if (c >= 0x20 and c <= 0x7f) {
                    continue;
                } else {
                    return false;
                }
            }
            return true;
        }

        void write_raw_features(writeable &w) {
            w.write_quote_enclosed_hex(val);
        }

        void write_json(struct json_object &o) {
            if (val.is_readable()) {
                if(is_printable_ascii()) {
                    o.print_key_json_string("value", val);
                } else {
                    o.print_key_hex("value_hex", val);
                }
            }
        }
    };

    // Lists are encoded as follows:
    // l<bencoded values>e
    //
    // Lists may contain any bencoded type, including integers, strings,
    // dictionaries, and even lists within other lists.

    // Integers are encoded as follows:
    // i<integer encoded in base ten ASCII>e
    //
    //
    class bint {
        literal_byte<'i'> start;
        one_or_more<digits> value;
        literal_byte<'e'> end;
        bool valid;

    public:
        bint(datum &d) :
             start(d),
             value(d),
             end(d),
             valid{d.is_not_null()} { }

        void write_raw_features(writeable &w) {
            w.write_quote_enclosed_hex(value.data, value.length());
        }

        void write_json(struct json_object &o) {
            if (!valid) {
                return;
            }

            o.print_key_json_string("value", value.data, value.length());
        }
    };

    class list_or_dict_end {
        literal_byte<'e'> end;

    public:
        list_or_dict_end(datum &d) : end{d} { }

    };

    // A list of values is encoded as l<contents>e . The contents
    // consist of the bencoded elements of the list, in order, concatenated.
    class blist {
        datum& body;
        bool valid;

    public:
        blist(datum &d) : body(d) {
            body.accept('l');
            valid = d.is_not_null();
        }

        bool is_not_empty() { return valid; }

        void write_raw_features(writeable &w);

        void write_json(struct json_object &o);
    };

    // Dictionaries are encoded as follows:
    //     d<bencoded string><bencoded element>e
    //
    // The initial d and trailing e are the beginning and ending
    // delimiters. Note that the keys must be bencoded strings. The
    // values may be any bencoded type, including integers, strings,
    // lists, and other dictionaries. Keys must be strings and appear
    // in sorted order (sorted as raw strings, not alphanumerics). The
    // strings should be compared using a binary comparison, not a
    // culture-specific "natural" comparison.
    //
    class dictionary {
        datum body;
        datum &tmp;
        bool valid;

    public:

        dictionary(datum &d) : tmp(d) {
            tmp.accept('d');
            body = tmp;
            valid = d.is_not_null();
        }

        bool is_not_empty() { return valid; }

        void write_raw_features(writeable &w);

        void write_json(struct json_object &o);

#ifndef NDEBUG

        inline bool unit_test() {
            unsigned char data[] = "d1:ad2:id20:abcdefghij012345678912:implied_porti1e9:info_hash20:mnopqrstuvwxyz1234564:porti-6881e5:token8:aoeusnthe1:q13:announce_peer1:t2:aa1:y1:qe";
            struct datum request_data{data, data + sizeof(data)};
            char buffer[8192];
            struct buffer_stream buf_json(buffer, sizeof(buffer));
            struct json_object record(&buf_json);


            dictionary dict{request_data};
            dict.write_json(record);
            return true;
        }
#endif //NDEBUG        
    };
};

class bencoded_data {
    datum &body;
    bool valid;

public:
    bencoded_data(datum &d) : body{d}, valid{d.is_not_null()} { }

    bool is_not_empty() { return valid; }

    void write_raw_features(writeable &w);

    void write_json(struct json_object &o);


};

#endif // BENCODE_H
