/* $OpenBSD: asn1object.c,v 1.1 2022/02/26 16:45:31 jsing Exp $ */
/*
 * Copyright (c) 2017, 2021, 2022 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <openssl/asn1.h>
#include <openssl/err.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

#include "asn1_locl.h"

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");

	fprintf(stderr, "\n");
}

static int
asn1_compare_bytes(const char *label, const unsigned char *d1, int len1,
    const unsigned char *d2, int len2)
{
	if (len1 != len2) {
		fprintf(stderr, "FAIL: %s - byte lengths differ "
		    "(%i != %i)\n", label, len1, len2);
		fprintf(stderr, "Got:\n");
		hexdump(d1, len1);
		fprintf(stderr, "Want:\n");
		hexdump(d2, len2);
		return 0;
	}
	if (memcmp(d1, d2, len1) != 0) {
		fprintf(stderr, "FAIL: %s - bytes differ\n", label);
		fprintf(stderr, "Got:\n");
		hexdump(d1, len1);
		fprintf(stderr, "Want:\n");
		hexdump(d2, len2);
		return 0;
	}
	return 1;
}

struct asn1_object_test {
	const char *oid;
	const char *txt;
	const uint8_t content[255];
	size_t content_len;
	const uint8_t der[255];
	size_t der_len;
	int want_error;
};

struct asn1_object_test asn1_object_tests[] = {
	{
		.oid = "2.5.4.10",
		.txt = "organizationName",
		.content = {
			0x55, 0x04, 0x0a,
		},
		.content_len = 3,
		.der = {
			0x06, 0x03, 0x55, 0x04, 0x0a,
		},
		.der_len = 5,
	},
	{
		.oid = "2 5 4 10",
		.txt = "organizationName",
		.content = {
			0x55, 0x04, 0x0a,
		},
		.content_len = 3,
		.der = {
			0x06, 0x03, 0x55, 0x04, 0x0a,
		},
		.der_len = 5,
	},
	{
		.oid = "1.3.6.1.4.1.11129.2.4.5",
		.txt = "CT Certificate SCTs",
		.content = {
			0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02,
			0x04, 0x05,
		},
		.content_len = 10,
		.der = {
			0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6,
			0x79, 0x02, 0x04, 0x05,
		},
		.der_len = 12,
	},
	{
		.oid = "2..5.4.10",
		.content = {
			0x00, 0x00, 0x00, 0x00,
		},
		.content_len = 4,
		.der = {
			0x06, 0x04, 0x00, 0x00, 0x00, 0x00,
		},
		.der_len = 6,
		.want_error = 0, /* XXX */
	},
	{
		.oid = "2.5..4.10",
		.content = {
			0x00, 0x00, 0x00, 0x00,
		},
		.content_len = 4,
		.der = {
			0x06, 0x04, 0x00, 0x00, 0x00, 0x00,
		},
		.der_len = 6,
		.want_error = 0, /* XXX */
	},
	{
		.oid = "2.5.4..10",
		.content = {
			0x00, 0x00, 0x00, 0x00,
		},
		.content_len = 4,
		.der = {
			0x06, 0x04, 0x00, 0x00, 0x00, 0x00,
		},
		.der_len = 6,
		.want_error = 0, /* XXX */
	},
	{
		.oid = "2.5.4.10.",
		.content = {
			0x00, 0x00, 0x00,
		},
		.content_len = 3,
		.der = {
			0x06, 0x03, 0x00, 0x00, 0x00,
		},
		.der_len = 5,
		.want_error = 0, /* XXX */
	},
	{
		.oid = "3.5.4.10",
		.want_error = ASN1_R_FIRST_NUM_TOO_LARGE,
	},
	{
		.oid = "2",
		.want_error = ASN1_R_MISSING_SECOND_NUMBER,
	},
	{
		.oid = "2,5,4,10",
		.want_error = ASN1_R_INVALID_SEPARATOR,
	},
};

#define N_ASN1_OBJECT_TESTS \
    (sizeof(asn1_object_tests) / sizeof(*asn1_object_tests))

static int
do_asn1_object_test(struct asn1_object_test *aot)
{
	ASN1_OBJECT *aobj = NULL;
	uint8_t buf[1024];
	const uint8_t *p;
	uint8_t *q;
	int err, ret;
	int failed = 1;

	ERR_clear_error();

	ret = a2d_ASN1_OBJECT(buf, sizeof(buf), aot->oid, -1);
	if (ret < 0 || (size_t)ret != aot->content_len) {
		fprintf(stderr, "FAIL: a2d_ASN1_OBJECT() = %d, want %zu\n",
		    ret, aot->content_len);
		goto failed;
	}
	if (aot->content_len == 0) {
		err = ERR_peek_error();
		if (ERR_GET_REASON(err) != aot->want_error) {
			fprintf(stderr, "FAIL: Got error reason %d, "
			    "want %d\n", ERR_GET_REASON(err),
			    aot->want_error);
			goto failed;
		}
		goto done;
	}

	p = aot->content;
	if ((aobj = c2i_ASN1_OBJECT(NULL, &p, aot->content_len)) == NULL) {
		fprintf(stderr, "FAIL: c2i_ASN1_OBJECT() failed\n");
		goto failed;
	}

	q = buf;
	ret = i2d_ASN1_OBJECT(aobj, &q);
	if (!asn1_compare_bytes("ASN1_OBJECT DER", buf, ret, aot->der,
	    aot->der_len))
		goto failed;

	ASN1_OBJECT_free(aobj);
	aobj = NULL;

	p = aot->der;
	if ((aobj = d2i_ASN1_OBJECT(NULL, &p, aot->der_len)) == NULL) {
		fprintf(stderr, "FAIL: d2i_ASN1_OBJECT() failed\n");
		goto failed;
	}

	if (aot->txt != NULL) {
		ret = i2t_ASN1_OBJECT(buf, sizeof(buf), aobj);
		if (ret <= 0 || (size_t)ret >= sizeof(buf)) {
			fprintf(stderr, "FAIL: i2t_ASN1_OBJECT() failed\n");
			goto failed;
		}
		if (strcmp(aot->txt, buf) != 0) {
			fprintf(stderr, "FAIL: i2t_ASN1_OBJECT() = '%s', "
			    "want '%s'\n", buf, aot->txt);
			goto failed;
		}
	}

 done:
	failed = 0;

 failed:
	ASN1_OBJECT_free(aobj);

	return failed;
}

static int
asn1_object_test(void)
{
	int failed = 0;
	size_t i;

	for (i = 0; i < N_ASN1_OBJECT_TESTS; i++)
		failed |= do_asn1_object_test(&asn1_object_tests[i]);

	return failed;
}

const uint8_t asn1_object_bad_content1[] = {
	0x55, 0x80, 0x04, 0x0a,
};
const uint8_t asn1_object_bad_content2[] = {
	0x55, 0x04, 0x8a,
};

static int
asn1_object_bad_content_test(void)
{
	ASN1_OBJECT *aobj = NULL;
	const uint8_t *p;
	size_t len;
	int failed = 1;

	p = asn1_object_bad_content1;
	len = sizeof(asn1_object_bad_content1); 
	if ((aobj = c2i_ASN1_OBJECT(NULL, &p, len)) != NULL) {
		fprintf(stderr, "FAIL: c2i_ASN1_OBJECT() succeeded with bad "
		    "content 1\n");
		goto failed;
	}

	p = asn1_object_bad_content2;
	len = sizeof(asn1_object_bad_content2); 
	if ((aobj = c2i_ASN1_OBJECT(NULL, &p, len)) != NULL) {
		fprintf(stderr, "FAIL: c2i_ASN1_OBJECT() succeeded with bad "
		    "content 2\n");
		goto failed;
	}

	failed = 0;

 failed:
	ASN1_OBJECT_free(aobj);

	return failed;
}

static int
asn1_object_txt_test(void)
{
	ASN1_OBJECT *aobj = NULL;
	uint8_t small_buf[2];
	const uint8_t *p;
	int err, len, ret;
	int failed = 1;

	ERR_clear_error();

	ret = a2d_ASN1_OBJECT(small_buf, sizeof(small_buf), "1.2.3.4", -1);
	if (ret != 0) {
		fprintf(stderr, "FAIL: a2d_ASN1_OBJECT() with small buffer "
		    "returned %d, want %d\n", ret, 0);
		goto failed;
	}
	err = ERR_peek_error();
	if (ERR_GET_REASON(err) != ASN1_R_BUFFER_TOO_SMALL) {
		fprintf(stderr, "FAIL: Got error reason %d, want %d\n",
		    ERR_GET_REASON(err), ASN1_R_BUFFER_TOO_SMALL);
		goto failed;
	}

	p = &asn1_object_tests[0].der[0];
	len = asn1_object_tests[0].der_len;
	aobj = d2i_ASN1_OBJECT(NULL, &p, len);
	if (aobj == NULL) {
		fprintf(stderr, "FAIL: d2i_ASN1_OBJECT() failed\n");
		goto failed;
	}
	ret = i2t_ASN1_OBJECT(small_buf, sizeof(small_buf), aobj);
	if (ret != 16) {
		fprintf(stderr, "FAIL: i2t_ASN1_OBJECT() with small buffer "
		    "returned %d, want %d\n", ret, 16);
		goto failed;
	}

	failed = 0;

 failed:
	ASN1_OBJECT_free(aobj);

	return failed;
}

const uint8_t asn1_large_oid_der[] = {
	0x06, 0x26,
	0x2b, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
};

static int
asn1_object_large_oid_test(void)
{
	ASN1_OBJECT *aobj = NULL;
	uint8_t buf[1024];
	const uint8_t *p;
	uint8_t *q;
	int ret;
	int failed = 1;

	failed = 0;

	p = asn1_large_oid_der;
	aobj = d2i_ASN1_OBJECT(NULL, &p, sizeof(asn1_large_oid_der));
	if (aobj == NULL) {
		fprintf(stderr, "FAIL: d2i_ASN1_OBJECT() failed with "
		    "large oid\n");
		goto failed;
	}

	q = buf;
	ret = i2d_ASN1_OBJECT(aobj, &q);
	if (!asn1_compare_bytes("ASN1_OBJECT DER", buf, ret, asn1_large_oid_der,
	    sizeof(asn1_large_oid_der)))
		goto failed;

 failed:
	ASN1_OBJECT_free(aobj);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= asn1_object_test();
	failed |= asn1_object_bad_content_test();
	failed |= asn1_object_txt_test();
	failed |= asn1_object_large_oid_test();

	return (failed);
}
