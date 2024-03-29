﻿#include <Nodos/PluginAPI.h>
#include <Nodos/PluginHelpers.hpp>
#include <nosUtil/Stopwatch.hpp>

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

#include "BluefishTypes_generated.h"
#include "Device.hpp"
#include "nosVulkanSubsystem/Helpers.hpp"

namespace bf
{
struct DMAWriteNodeContext : nos::NodeContext
{
	DMAWriteNodeContext(const nosFbNode* node) : NodeContext(node)
	{
	}

	nosResult ExecuteNode(const nosNodeExecuteArgs* args) override
	{
		nos::bluefish::ChannelInfo* channelInfo = nullptr;
		nosResourceShareInfo inputBuffer{};
		for (size_t i = 0; i < args->PinCount; ++i)
		{
			auto& pin = args->Pins[i];
			if (pin.Name == NOS_NAME_STATIC("Channel"))
				channelInfo = nos::InterpretPinValue<nos::bluefish::ChannelInfo>(*pin.Data);
			if (pin.Name == NOS_NAME_STATIC("Input"))
				inputBuffer = nos::vkss::ConvertToResourceInfo(*nos::InterpretPinValue<nos::sys::vulkan::Buffer>(*pin.Data));
		}
		
		if (!inputBuffer.Memory.Handle)
			return NOS_RESULT_FAILED;
 
		if (!channelInfo->device() || !channelInfo->channel())
			return NOS_RESULT_FAILED;

		auto device = BluefishDevice::GetDevice(channelInfo->device()->serial()->str());
		if (!device)
			return NOS_RESULT_FAILED;
		auto channel = static_cast<EBlueVideoChannel>(channelInfo->channel()->id());
		std::string channelStr = bfcUtilsGetStringForVideoChannel(channel);
		auto d = device->GetDeltaSeconds(channel);
		nosVec2u newDeltaSeconds = { d[0], d[1] };
		if (memcmp(&newDeltaSeconds, &DeltaSeconds, sizeof(nosVec2u)) != 0)
		{
			// TODO: Recompilation needed. Nodos should call our GetScheduleInfo function.
			DeltaSeconds = newDeltaSeconds;
		}

		nosCmd cmd;
		nosVulkan->Begin("Flush Before Bluefish DMA Write", &cmd);
		nosGPUEvent event;
		nosCmdEndParams end {.ForceSubmit = NOS_TRUE, .OutGPUEventHandle = &event};
		nosVulkan->End(cmd, &end);
		auto res = nosVulkan->WaitGpuEvent(&event, 10e9);
		if (res != NOS_RESULT_SUCCESS)
			nosEngine.LogE("Error when flush before Bluefish DMA write");
		auto buffer = nosVulkan->Map(&inputBuffer);

		{
			nos::util::Stopwatch sw;
			if((uintptr_t)buffer % 64 != 0)
				nosEngine.LogE("DMA write only accepts buffers addresses to be aligned to 64 bytes"); // TODO: Check device. This is only in Khronos range!
			device->DMAWriteFrame(channel, BufferId, buffer, inputBuffer.Info.Buffer.Size);
			auto elapsed = sw.Elapsed();
			nosEngine.WatchLog(("Bluefish " + channelStr + " DMA Write").c_str(), nos::util::Stopwatch::ElapsedString(elapsed).c_str());
		}

		BufferId = (BufferId + 1) % 4;

		nosScheduleNodeParams schedule{
			.NodeId = NodeId,
			.AddScheduleCount = 1
		};
		nosEngine.ScheduleNode(&schedule);

		return NOS_RESULT_SUCCESS;
	}
 
	void GetScheduleInfo(nosScheduleInfo* out) override
	{
		*out = nosScheduleInfo {
			.Importance = 1,
			.DeltaSeconds = DeltaSeconds,
			.Type = NOS_SCHEDULE_TYPE_ON_DEMAND,
		};
	}
	
	void OnPathStart() override
	{
		nosScheduleNodeParams schedule{.NodeId = NodeId, .AddScheduleCount = 1};
		nosEngine.ScheduleNode(&schedule);
	}

	nosVec2u DeltaSeconds{};
	BLUE_U32 BufferId = 0;
};

nosResult RegisterDMAWriteNode(nosNodeFunctions* outFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("DMAWrite"), DMAWriteNodeContext, outFunctions)
	return NOS_RESULT_SUCCESS;
}
}