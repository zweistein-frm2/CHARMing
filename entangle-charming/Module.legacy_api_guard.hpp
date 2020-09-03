namespace {
    struct legacy_api_guard
    {
        legacy_api_guard() { initialize(); }
        ~legacy_api_guard() { shutdown(); }
    };

    /// @brief Global shared guard for the legacy API.
    boost::shared_ptr<legacy_api_guard> legacy_api_guard_;
    /// @brief Get (or create) guard for legacy API.
    boost::shared_ptr<legacy_api_guard> get_api_guard()
    {
        if (!legacy_api_guard_)
        {
            legacy_api_guard_ = boost::make_shared<legacy_api_guard>();
        }
        return legacy_api_guard_;
    }

    void release_guard()
    {
        try {
            LOG_DEBUG << "release_guard()" << std::endl;
        }
        catch (std::exception&) {}
        legacy_api_guard_.reset();
    }



} // namespace