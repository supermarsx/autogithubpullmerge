#define private public
#include "github_client.hpp"
#undef private
#include <catch2/catch_test_macros.hpp>
#include <curl/curl.h>
#include <string>

using namespace agpm;

TEST_CASE("CurlHttpClient configuration") {
  curl_off_t download_limit = 123;
  curl_off_t upload_limit = 456;
  curl_off_t max_download = 789;
  curl_off_t max_upload = 1011;
  std::string http_proxy = "http://proxy";
  std::string https_proxy = "http://secureproxy";
  CurlHttpClient client(1000, download_limit, upload_limit, max_download,
                        max_upload, http_proxy, https_proxy);
  REQUIRE(client.download_limit_ == download_limit);
  REQUIRE(client.upload_limit_ == upload_limit);
  REQUIRE(client.max_download_ == max_download);
  REQUIRE(client.max_upload_ == max_upload);
  REQUIRE(client.http_proxy_ == http_proxy);
  REQUIRE(client.https_proxy_ == https_proxy);
}
