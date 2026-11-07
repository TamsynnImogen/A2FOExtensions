#pragma once
#include "presentation.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace a2fo::refit_menu::menu_scene {
// This lock covers only small request/snapshot exchanges. Driver calls, model
// loading, shader compilation, readback and logging must happen outside it.
class Mailbox {
public:
    struct Request {
        bool active = false;
        bool stop = false;
        std::string view = "main";
        bool departure = false;
        std::uint64_t epoch = 0;
        std::uint64_t lease_until = 0;
        int width = 640;
        int height = 360;
        std::uint64_t revision = 0;
        std::uint64_t frame = 0;
        double animation_time = 1.0;
    };
    struct Snapshot {
        std::shared_ptr<const Artwork> image;
        std::uint64_t revision = 0;
        std::uint64_t image_revision = 0;
        bool loading = false;
        bool departure = false;
        std::uint64_t epoch = 0;
        std::string view;
        std::uint64_t frame = 0;
    };
    bool request(std::string view, int width, int height, bool active,
                 bool departure = false, std::uint64_t lease_until = 0, bool restart = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (request_.stop) return false;
        request_.lease_until = departure ? lease_until : 0;
        if (request_.view == view && request_.width == width &&
            request_.height == height && request_.active == active &&
            request_.departure == departure && !restart) return false;
        if (request_.departure != departure || restart) ++request_.epoch;
        if (request_.view != view || request_.departure != departure || restart) consumed_at_ = 0;
        request_.view = std::move(view);
        request_.departure = departure;
        request_.width = width;
        request_.height = height;
        request_.active = active;
        ++request_.revision;
        return true;
    }
    bool active(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);
        // An old submenu timer/destructor must not pause a loading departure.
        if (request_.stop || request_.departure || request_.active == value) return false;
        request_.active = value;
        ++request_.revision;
        return true;
    }
    bool end_departure() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (request_.stop || !request_.departure) return false;
        request_.departure = false;
        request_.active = false;
        request_.lease_until = 0;
        ++request_.epoch;
        ++request_.revision;
        return true;
    }
    Request request() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return request_;
    }
    Snapshot snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_;
    }
    // Allocate before locking; existing UI snapshots remain immutable and valid.
    bool publish(Artwork image, const Request& rendered) {
        auto owned = std::make_shared<const Artwork>(std::move(image));
        std::lock_guard<std::mutex> lock(mutex_);
        if (request_.stop || request_.view != rendered.view ||
            request_.width != rendered.width || request_.height != rendered.height ||
            request_.departure != rendered.departure || request_.epoch != rendered.epoch ||
            request_.frame != rendered.frame) return false;
        snapshot_.image = std::move(owned);
        snapshot_.loading = false;
        snapshot_.departure = rendered.departure;
        snapshot_.epoch = rendered.epoch;
        snapshot_.view = rendered.view;
        snapshot_.frame = rendered.frame;
        ++snapshot_.revision;
        ++snapshot_.image_revision;
        return true;
    }
    // One completed frame may wait for presentation. Do not keep rendering and
    // discarding a wall-clock animation while the engine is busy loading assets.
    bool awaiting_presentation(const Request& request) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_.image && snapshot_.view == request.view &&
            snapshot_.image->width == request.width && snapshot_.image->height == request.height &&
            snapshot_.departure == request.departure && snapshot_.epoch == request.epoch &&
            snapshot_.frame == request.frame;
    }
    bool needs_presentation(std::uint64_t shown_revision) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_.image && snapshot_.image_revision!=shown_revision &&
            snapshot_.view==request_.view && snapshot_.departure==request_.departure &&
            snapshot_.epoch==request_.epoch && snapshot_.frame==request_.frame &&
            snapshot_.image->width==request_.width && snapshot_.image->height==request_.height;
    }
    bool presented(const std::shared_ptr<const Artwork>& image, std::uint64_t now,
                   std::uint64_t maximum_step = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (request_.stop || !image || image != snapshot_.image ||
            image->width != request_.width || image->height != request_.height ||
            snapshot_.frame != request_.frame || snapshot_.view != request_.view ||
            snapshot_.epoch != request_.epoch || snapshot_.departure != request_.departure) return false;
        // A native loading step may block for seconds. Retain the choreography
        // across that stall rather than jumping over the camera and ship motion.
        const auto elapsed = consumed_at_ && now >= consumed_at_ ? now - consumed_at_ : 33;
        request_.animation_time += static_cast<double>(elapsed < maximum_step ? elapsed : maximum_step) / 1000.0;
        consumed_at_ = now;
        ++request_.frame;
        return true;
    }
    void loading(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.loading == value) return;
        snapshot_.loading = value;
        ++snapshot_.revision;
    }
    void log(std::string message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (logs_.size() < 32) logs_.push_back(std::move(message));
    }
    std::vector<std::string> logs() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        result.swap(logs_);
        return result;
    }
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        request_.stop = true;
        request_.active = false;
        ++request_.revision;
    }
private:
    mutable std::mutex mutex_;
    Request request_;
    Snapshot snapshot_;
    std::uint64_t consumed_at_ = 0;
    std::vector<std::string> logs_;
};
}
