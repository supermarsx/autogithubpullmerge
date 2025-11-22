// Flush cache immediately (thread-safe public API)
void GitHubClient::flush_cache() {
  std::scoped_lock lock(mutex_);
  save_cache_locked();
}

void GitHubClient::set_cache_flush_interval(std::chrono::milliseconds interval) {
  cache_flush_interval_ = interval;
  // wake flusher so it picks up new interval promptly
  cache_flusher_cv_.notify_all();
}
