#ifndef MUSIKCORE_PLAYER_HPP
#define MUSIKCORE_PLAYER_HPP
#include <musikcore/context.hpp>
#include <musikcore/player.h>
#include <utility>

namespace musik { namespace core { namespace api {
    /* owns a URI playback resource. noncopyable; moves transfer ownership and
    empty the source. use with a context owning Plugins, without needing a
    catalog. destroy/move on the owner thread outside player callbacks. context
    shutdown stops playback; retained wrappers remain safely destroyable. */
    class Player {
        public:
            Player() noexcept = default;
            explicit Player(Context& context) {
                const auto status = mc_player_create(context.Get(), &this->player);
                if (status != MC_OK) {
                    throw Error(status);
                }
            }
            Player(const Player&) = delete;
            Player(Player&& other) noexcept
            : player(std::exchange(other.player, nullptr)) {}
            ~Player() { mc_player_destroy(&this->player); }

            Player& operator=(const Player&) = delete;
            Player& operator=(Player&& other) {
                if (this != &other) {
                    const auto status = this->Close();
                    if (status != MC_OK) {
                        throw Error(status);
                    }
                    this->player = std::exchange(other.player, nullptr);
                }
                return *this;
            }

            mc_result Play(std::string_view uri) { return mc_player_play_uri(this->player, { uri.data(), uri.size() }); }
            mc_result Pause() { return mc_player_pause(this->player); }
            mc_result Resume() { return mc_player_resume(this->player); }
            mc_result Stop() { return mc_player_stop(this->player); }
            mc_result Seek(double seconds) { return mc_player_seek(this->player, seconds); }
            mc_result SetVolume(double volume) { return mc_player_set_volume(this->player, volume); }
            mc_result SetCallback(mc_player_changed_fn callback, void* data = nullptr) {
                return mc_player_set_callback(this->player, callback, data);
            }
            mc_player_snapshot Snapshot() const {
                mc_player_snapshot result = MC_PLAYER_SNAPSHOT_DEFAULT;
                const auto status = mc_player_get_snapshot(this->player, &result);
                if (status != MC_OK) {
                    throw Error(status);
                }
                return result;
            }
            mc_result Close() { return mc_player_destroy(&this->player); }
            mc_player* Get() noexcept { return this->player; }
            const mc_player* Get() const noexcept { return this->player; }

        private:
            mc_player* player = nullptr;
    };
} } }
#endif
