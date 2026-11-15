#ifndef ENGINEHOLDER_H
#define ENGINEHOLDER_H

#include "SearchEngine.h"
#include <memory>
#include <mutex>
#include <string>

class EngineHolder
{
public:
    std::shared_ptr<const SearchEngine> get() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return engine_;
    }

    void set(std::shared_ptr<const SearchEngine> engine, const std::string& dataVersion)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        engine_ = std::move(engine);
        version_ = dataVersion;
    }

    std::string version() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return version_;
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const SearchEngine> engine_;
    std::string version_;
};

#endif
