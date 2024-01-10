#include <cstdio>
#include <filesystem>
#include <memory>

#include <nba/config.hpp>
#include <nba/device/audio_device.hpp>
#include <nba/rom/rom.hpp>
#include <nba/core.hpp>
#include <nba/device/video_device.hpp>
#include <platform/loader/rom.hpp>
#include <platform/loader/bios.hpp>

#include <hydra/core.hxx>
#include "icon.h"

static constexpr int gba_screen_width  = 240;
static constexpr int gba_screen_height = 160;

struct HydraVideoDevice final : public nba::VideoDevice
{
    void Draw(u32* buffer);
    void SetVideoCallback(void (*callback)(void* data, hydra::Size size)) { video_callback = callback; }

private:
    void (*video_callback)(void* data, hydra::Size size) = nullptr;
};

class HydraCore final : public hydra::IBase, public hydra::ISoftwareRendered, public hydra::IFrontendDriven, public hydra::IConfigurable
{
    HYDRA_CLASS
public:
    HydraCore()
    {
        video_device = std::make_shared<HydraVideoDevice>();
    }
    bool loadFile(const char* type, const char* path) override;
    void reset() override;
    hydra::Size getNativeSize() override;
    void setOutputSize(hydra::Size size) override;
    void setVideoCallback(void (*callback)(void* data, hydra::Size size)) override;
    hydra::PixelFormat getPixelFormat() override { return hydra::PixelFormat::BGRA; }
    void runFrame() override { core->RunForOneFrame(); }
    uint16_t getFps() override { return 60; }
    void setSetCallback(void (*callback)(const char *, const char *)) override {}
    void setGetCallback(const char* (*callback)(const char *)) override { get_callback = callback; }

    std::unique_ptr<nba::CoreBase> core;
    std::shared_ptr<HydraVideoDevice> video_device;

    bool bios_loaded = false;
    const char* (*get_callback)(const char *) = nullptr;
};

bool HydraCore::loadFile(const char* type, const char* path)
{
    if (!core)
    {
        std::shared_ptr<nba::Config> config = std::make_shared<nba::Config>();
        config->video_dev = video_device;
        config->audio_dev = std::make_shared<nba::NullAudioDevice>();
        config->input_dev = std::make_shared<nba::NullInputDevice>();
        config->skip_bios = get_callback("skip_bios") == std::string("true");
        core = nba::CreateCore(config);
    }

    std::string type_str(type);
    if (type_str == "rom")
    {
        std::filesystem::path fspath(path);
        std::filesystem::path save_path = fspath.parent_path() / (fspath.stem().string() + ".sav");
        auto result = nba::ROMLoader::Load(core, path, save_path.string());

        switch(result) {
            case nba::ROMLoader::Result::CannotFindFile: {
                printf("Cannot find ROM\n");
                return false;
            }
            case nba::ROMLoader::Result::CannotOpenFile:
            case nba::ROMLoader::Result::BadImage: {
                printf("Failed to load ROM\n");
                return false;
            }
            case nba::ROMLoader::Result::Success: {
                if (!bios_loaded)
                {
                    printf("No BIOS loaded\n");
                }
                return bios_loaded;
            }
        }
    } else if (type_str == "bios")
    {
        auto result = nba::BIOSLoader::Load(core, path);
        
        switch (result) {
            case nba::BIOSLoader::Result::CannotFindFile: {
                printf("Cannot find BIOS\n");
                return false;
            }
            case nba::BIOSLoader::Result::CannotOpenFile:
            case nba::BIOSLoader::Result::BadImage: {
                printf("Failed to load BIOS\n");
                return false;
            }
            case nba::BIOSLoader::Result::Success: {
                bios_loaded = true;
                return true;
            }
        }
        return true;
    }

    printf("Unknown file type: %s\n", type);
    return false;
}

void HydraCore::reset()
{
    core->Reset();
}

hydra::Size HydraCore::getNativeSize()
{
    return {gba_screen_width, gba_screen_height};
}

void HydraCore::setOutputSize(hydra::Size size)
{
    // TODO: Do we do anything with this size hint?
}

void HydraCore::setVideoCallback(void (*callback)(void* data, hydra::Size size))
{
    video_device->SetVideoCallback(callback);
}

void HydraVideoDevice::Draw(u32* buffer)
{
    std::ofstream ofs("/home/offtkp/test.raw", std::ios::binary);
    ofs.write((char*)buffer, gba_screen_width * gba_screen_height * sizeof(u32));
    if (video_callback)
    {
        video_callback(buffer, {gba_screen_width, gba_screen_height});
    }
}

HC_API hydra::IBase* createEmulator()
{
    return new HydraCore();
}

HC_API void destroyEmulator(hydra::IBase* emulator)
{
    delete emulator;
}

HC_API const char* getInfo(hydra::InfoType type)
{
    switch (type)
    {
        case hydra::InfoType::CoreName:
            return "NanoBoyAdvance";
        case hydra::InfoType::SystemName:
            return "Game Boy Advance";
        case hydra::InfoType::Author:
            return "fleroviux";
        case hydra::InfoType::Description:
            return "A cycle-accurate Nintendo Game Boy Advance emulator.";
        case hydra::InfoType::Version:
            return "1.7";
        case hydra::InfoType::License:
            return "GPLv3";
        case hydra::InfoType::Extensions:
            return "gba";
        case hydra::InfoType::Website:
            return "https://github.com/nba-emu/NanoBoyAdvance";
        case hydra::InfoType::IconData:
            return (const char*)nba_icon;
        case hydra::InfoType::IconWidth:
        case hydra::InfoType::IconHeight:
            return "128";
        case hydra::InfoType::Settings:
            return R"!(
                [bios]
                type = "filepicker"
                name = "GBA BIOS"
                description = "The GBA BIOS is required to run games."
                required = true

                [skip_bios]
                type = "checkbox"
                name = "Skip BIOS"
                description = "Skip the BIOS screen and boot directly into the game."
                default = false
            )!";
        default:
            return nullptr;
    }
}
