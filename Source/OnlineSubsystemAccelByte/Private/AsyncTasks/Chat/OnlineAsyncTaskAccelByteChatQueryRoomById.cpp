// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteChatQueryRoomById.h"

FOnlineAsyncTaskAccelByteChatQueryRoomById::FOnlineAsyncTaskAccelByteChatQueryRoomById(
	FOnlineSubsystemAccelByte* const InABInterface,
	const FUniqueNetId& InLocalUserId,
	const FChatRoomId& InRoomId,
	const FOnChatQueryRoomByIdComplete& InDelegate)
	: FOnlineAsyncTaskAccelByte(InABInterface), RoomId(InRoomId), Delegate(InDelegate)
{
	UserId = StaticCastSharedRef<const FUniqueNetIdAccelByteUser>(InLocalUserId.AsShared());
}

void FOnlineAsyncTaskAccelByteChatQueryRoomById::Initialize()
{
	Super::Initialize();
	
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	const AccelByte::Api::Chat::FQueryTopicByIdResponse OnQueryRoomSuccessDelegate =
		AccelByte::Api::Chat::FQueryTopicByIdResponse::CreateRaw(this, &FOnlineAsyncTaskAccelByteChatQueryRoomById::OnQueryRoomSuccess);
	const FErrorHandler OnQueryRoomErrorDelegate = FErrorHandler::CreateRaw(this, &FOnlineAsyncTaskAccelByteChatQueryRoomById::OnQueryRoomError);

	ApiClient->Chat.QueryTopicById(RoomId, OnQueryRoomSuccessDelegate, OnQueryRoomErrorDelegate);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteChatQueryRoomById::TriggerDelegates()
{
	Super::TriggerDelegates();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));
	
	Delegate.ExecuteIfBound(bWasSuccessful, RoomInfo, LocalUserNum);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteChatQueryRoomById::Finalize()
{
	Super::Finalize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	FOnlineChatAccelBytePtr ChatInterface;
	if (ensure(FOnlineChatAccelByte::GetFromSubsystem(Subsystem, ChatInterface)))
	{
		if (bWasSuccessful)
		{
			ChatInterface->AddChatRoomMembers(FoundUsers);
		}
	}
	else
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Failed to set chat room member chat interface instance is not valid!"));
	}
	
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteChatQueryRoomById::OnQueryRoomError(int32 ErrorCode, const FString& ErrorMessage)
{
	UE_LOG_AB(Warning, TEXT("Failed to query room. Error code: %d; Error message: %s"), ErrorCode, *ErrorMessage);
	ErrorString = ErrorMessage;
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}

void FOnlineAsyncTaskAccelByteChatQueryRoomById::OnQueryRoomSuccess(const FAccelByteModelsChatQueryTopicByIdResponse& Response)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Processed: %s"), *Response.Processed.ToIso8601());

	SetLastUpdateTimeToCurrentTime();

	RoomInfo = FAccelByteChatRoomInfo::Create();
	RoomInfo->SetTopicData(Response.Data);

	FOnlineChatAccelBytePtr ChatInterface;
	if (ensure(FOnlineChatAccelByte::GetFromSubsystem(Subsystem, ChatInterface)))
	{
		// add to cache immediately to avoid race condition
		ChatInterface->AddTopic(RoomInfo.ToSharedRef());
	}
	
	TArray<FString> UserIds;
	for (const auto& Id : Response.Data.Members)
	{
		UserIds.AddUnique(Id);
	}

	FOnlineUserCacheAccelBytePtr UserStore = Subsystem->GetUserCache();
	if (!UserStore.IsValid())
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Could not query chat room by id %s as our user store instance is invalid!"), *RoomId);
		CompleteTask(EAccelByteAsyncTaskCompleteState::InvalidState);
		return;
	}

	FOnQueryUsersComplete OnQueryUsersCompleteDelegate = FOnQueryUsersComplete::CreateRaw(this, &FOnlineAsyncTaskAccelByteChatQueryRoomById::OnQueryMemberInformationComplete);
	UserStore->QueryUsersByAccelByteIds(LocalUserNum, UserIds, OnQueryUsersCompleteDelegate);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteChatQueryRoomById::OnQueryMemberInformationComplete(bool bIsSuccessful, TArray<TSharedRef<FAccelByteUserInfo>> UsersQueried)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Length: %d"), UsersQueried.Num());
	SetLastUpdateTimeToCurrentTime();

	FoundUsers = UsersQueried;
	
	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}