#pragma once

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif
#include <BlueVelvetC.h>
#include <BlueVelvetCUtils.h>

// stl
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <array>

namespace bf
{

class BluefishDevice
{
public:
	static BErr InitializeDevices();
	static std::shared_ptr<BluefishDevice> GetDevice(std::string const& serial);
	static std::shared_ptr<BluefishDevice> GetDevice(BLUE_S32 id);
	static void ForEachDevice(std::function<void(BluefishDevice&)>&& func);
	
	BluefishDevice(BLUE_S32 deviceId, BErr& error);
	~BluefishDevice();

	bool CanChannelDoInput(EBlueVideoChannel channel) const;
	blue_setup_info GetRecommendedSetupInfoForInput(EBlueVideoChannel channel, BErr& err) const;
    BErr OpenChannel(EBlueVideoChannel channel, EVideoModeExt mode);
	void CloseChannel(EBlueVideoChannel channel);
	bool DMAWriteFrame(EBlueVideoChannel channel, uint32_t bufferId, uint8_t* buffer, uint32_t size) const;
	void WaitForOutputVBI(EBlueVideoChannel channel, unsigned long& fieldCount) const;
	std::array<uint32_t, 2> GetDeltaSeconds(EBlueVideoChannel channel) const;
	
	std::string GetSerial() const;
	BLUE_S32 GetId() const { return Id; }
	std::string GetName() const;
	blue_device_info const& GetInfo() const { return Info; }
private:
	inline static std::unordered_map<std::string, std::shared_ptr<BluefishDevice>> Devices = {};

	BLUEVELVETC_HANDLE Instance = nullptr;
	BLUE_S32 Id = 0;
	blue_device_info Info{};

	std::unordered_map<EBlueVideoChannel, std::unique_ptr<class Channel>> Channels;
};

class Channel
{
public:
	Channel(BluefishDevice* device, EBlueVideoChannel channel, EVideoModeExt mode, BErr& err);
	~Channel();

	Channel(Channel const&) = delete;
	
	bool DMAWriteFrame(uint32_t bufferId, uint8_t* buffer, uint32_t size) const;
	void WaitForOutputVBI(unsigned long& fieldCount) const;
	std::array<uint32_t, 2> GetDeltaSeconds() const { return DeltaSeconds; }
	
protected:
	BluefishDevice* Device;
	BLUEVELVETC_HANDLE Instance = nullptr;
	std::array<uint32_t, 2> DeltaSeconds;
};

inline void ReplaceString(std::string &str, const std::string &toReplace, const std::string &replacement) {
	if (toReplace.empty()) return;

	size_t startPos = 0;
	while((startPos = str.find(toReplace, startPos)) != std::string::npos) {
		str.replace(startPos, toReplace.length(), replacement);
		startPos += replacement.length();
	}
}

inline uint8_t ParseChannelNumber(std::string channelName)
{
	ReplaceString(channelName, "Input Ch", "");
	ReplaceString(channelName, "Output Ch", "");
	return std::atoi(channelName.c_str());
}

inline uint8_t GetChannelNumber(EBlueVideoChannel channel)
{
	return ParseChannelNumber(bfcUtilsGetStringForVideoChannel(channel));
}

inline bool IsInputChannel(EBlueVideoChannel ch)
{
	return ch == BLUE_VIDEO_INPUT_CHANNEL_1 || ch == BLUE_VIDEO_INPUT_CHANNEL_2 || ch == BLUE_VIDEO_INPUT_CHANNEL_3 ||
		   ch == BLUE_VIDEO_INPUT_CHANNEL_4 || ch == BLUE_VIDEO_INPUT_CHANNEL_5 || ch == BLUE_VIDEO_INPUT_CHANNEL_6 ||
		   ch == BLUE_VIDEO_INPUT_CHANNEL_7 || ch == BLUE_VIDEO_INPUT_CHANNEL_8;
}

}