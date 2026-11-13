#include "features/notification_support/identity.hpp"
#include "features/notification_support/text_validation.hpp"
#include "support/test_harness.hpp"

#include <string>

using namespace oneintwo;
using namespace oneintwo::tests;

TEST_CASE(utf8_validation_accepts_well_formed_text) {
    CHECK(notification_support::is_valid_utf8(""));
    CHECK(notification_support::is_valid_utf8("plain ASCII"));
    CHECK(notification_support::is_valid_utf8("caf\xC3\xA9"));
    CHECK(notification_support::is_valid_utf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"));
    CHECK(notification_support::is_valid_utf8("\xF0\x9F\x98\x80"));
    CHECK(notification_support::is_valid_utf8(std::string("a\0b", 3)));
}

TEST_CASE(utf8_validation_rejects_malformed_sequences) {
    CHECK(!notification_support::is_valid_utf8("\x80"));
    CHECK(!notification_support::is_valid_utf8("\xC3"));
    CHECK(!notification_support::is_valid_utf8("\xE2\x82"));
    CHECK(!notification_support::is_valid_utf8("\xC0\xAF"));         // sobrelarga
    CHECK(!notification_support::is_valid_utf8("\xED\xA0\x80"));     // sustituto
    CHECK(!notification_support::is_valid_utf8("\xF4\x90\x80\x80")); // > U+10FFFF
    CHECK(!notification_support::is_valid_utf8("\xF8\x88\x80\x80\x80"));
    CHECK(!notification_support::is_valid_utf8("\xC3\x28"));
}

TEST_CASE(blank_title_detection_uses_ascii_whitespace) {
    CHECK(notification_support::is_blank(""));
    CHECK(notification_support::is_blank(" \t\n\r"));
    CHECK(!notification_support::is_blank(" a "));
    CHECK(!notification_support::is_blank("\xC2\xA0"));
}

TEST_CASE(plain_text_sanitisation_preserves_unicode_and_newlines) {
    const std::string sanitized = notification_support::sanitize_plain_text(
        std::string("A\x01" "B\tC\nD\x7F") + "\xF0\x9F\x98\x80");
    CHECK_EQUAL(sanitized, std::string("A B\tC\nD ") + "\xF0\x9F\x98\x80");
}

TEST_CASE(product_identity_excludes_only_the_oneintwo_family) {
    CHECK(notification_support::is_product_application("OneInTwo.Desktop"));
    CHECK(notification_support::is_product_application("oneintwo.desktop"));
    CHECK(notification_support::is_product_application("OneInTwo"));
    CHECK(notification_support::is_product_application(
        "OneInTwo.Desktop_1234567890abc!OneInTwo.Desktop"));
    CHECK(!notification_support::is_product_application(""));
    CHECK(!notification_support::is_product_application("notoneintwo.desktop"));
    CHECK(!notification_support::is_product_application("slack.exe"));
}
