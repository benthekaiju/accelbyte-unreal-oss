// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteConnectChat.h"

#include "AsyncTasks/OnlineAsyncTaskAccelByteUtils.h"

FOnlineAsyncTaskAccelByteConnectChat::FOnlineAsyncTaskAccelByteConnectChat(
	FOnlineSubsystemAccelByte* const InABInterface,
	const FUniqueNetId& InLocalUserId)
	: FOnlineAsyncTaskAccelByte(InABInterface)
{
	UserId = FUniqueNetIdAccelByteUser::CastChecked(InLocalUserId);
	ErrorStr = TEXT("");
}

void FOnlineAsyncTaskAccelByteConnectChat::Initialize()
{
	Super::Initialize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("UserId: %s"), *UserId->ToDebugString());

	// Create delegates for successfully as well as unsuccessfully connecting to the AccelByte lobby websocket
	OnChatConnectSuccessDelegate = TDelegateUtils<AccelByte::Api::Chat::FChatConnectSuccess>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteConnectChat::OnChatConnectSuccess);
	OnChatConnectErrorDelegate = TDelegateUtils<FErrorHandler>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteConnectChat::OnChatConnectError);

	OnChatDisconnectedNotifDelegate = TDelegateUtils<AccelByte::Api::Chat::FChatDisconnectNotif>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteConnectChat::OnChatDisconnectedNotif);
	ApiClient->Chat.SetDisconnectNotifDelegate(OnChatDisconnectedNotifDelegate);

	// Send off a request to connect to the lobby websocket, as well as connect our delegates for doing so
	ApiClient->Chat.SetConnectSuccessDelegate(OnChatConnectSuccessDelegate);
	ApiClient->Chat.SetConnectFailedDelegate(OnChatConnectErrorDelegate);
	ApiClient->Chat.Connect();

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteConnectChat::TriggerDelegates()
{
	Super::TriggerDelegates();
	
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	const FOnlineChatAccelBytePtr ChatInterface = StaticCastSharedPtr<FOnlineChatAccelByte>(Subsystem->GetChatInterface());
	if (ChatInterface.IsValid())
	{
		ChatInterface->TriggerOnConnectChatCompleteDelegates(LocalUserNum, bWasSuccessful, *UserId.Get(), ErrorStr);
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteConnectChat::OnChatConnectSuccess()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));
	
	// set chat delegates to interface events
	const FOnlineChatAccelBytePtr ChatInterface = StaticCastSharedPtr<FOnlineChatAccelByte>(Subsystem->GetChatInterface());
	if (ChatInterface.IsValid())
	{
		ChatInterface->RegisterChatDelegates(UserId.ToSharedRef().Get());
	}

	// Get identity interface and set connected to chat
	const FOnlineIdentityAccelBytePtr IdentityInterface = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
	if (IdentityInterface.IsValid())
	{
		const TSharedPtr<const FUniqueNetId> UserIdPtr = IdentityInterface->GetUniquePlayerId(LocalUserNum);
		TSharedPtr<FUserOnlineAccount> UserAccount = IdentityInterface->GetUserAccount(UserId.ToSharedRef().Get());

		if (UserAccount.IsValid())
		{
			const TSharedPtr<FUserOnlineAccountAccelByte> UserAccountAccelByte = StaticCastSharedPtr<FUserOnlineAccountAccelByte>(UserAccount);
			UserAccountAccelByte->SetConnectedToChat(true);
		}
	}
	
	// set connection closed delegates to set chat connected false when connection closed is triggered
	const Api::Chat::FChatConnectionClosed OnChatConnectionClosedDelegate = Api::Chat::FChatConnectionClosed::CreateStatic(FOnlineAsyncTaskAccelByteConnectChat::OnChatConnectionClosed, LocalUserNum, IdentityInterface);
	ApiClient->Chat.SetConnectionClosedDelegate(OnChatConnectionClosedDelegate);

	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteConnectChat::OnChatConnectError(int32 ErrorCode, const FString& ErrorMessage)
{
	ErrorStr = TEXT("login-failed-chat-connect-error");
	UE_LOG_AB(Warning, TEXT("Failed to connect to the AccelByte chat websocket! Error Code: %d; Error Message: %s"), ErrorCode, *ErrorMessage);
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}

void FOnlineAsyncTaskAccelByteConnectChat::OnChatDisconnectedNotif(const FAccelByteModelsChatDisconnectNotif& Result)
{
	// Update identity interface chat flag
	const TSharedPtr<FOnlineIdentityAccelByte, ESPMode::ThreadSafe> IdentityInterface = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
	if (IdentityInterface.IsValid())
	{
		const TSharedPtr<const FUniqueNetId> UserIdPtr = IdentityInterface->GetUniquePlayerId(LocalUserNum);
		TSharedPtr<FUserOnlineAccount> UserAccount = IdentityInterface->GetUserAccount(UserId.ToSharedRef().Get());

		if (UserAccount.IsValid())
		{
			const TSharedPtr<FUserOnlineAccountAccelByte> UserAccountAccelByte = StaticCastSharedPtr<FUserOnlineAccountAccelByte>(UserAccount);
			UserAccountAccelByte->SetConnectedToChat(false);
		}
	}

	ErrorStr = TEXT("login-failed-chat-connect-error");

	UnbindDelegates();

	UE_LOG_AB(Warning, TEXT("Chat disconnected. Reason '%s'"), *Result.Message);
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}

void FOnlineAsyncTaskAccelByteConnectChat::OnChatConnectionClosed(int32 StatusCode, const FString& Reason,
	bool WasClean, int32 InLocalUserNum, const FOnlineIdentityAccelBytePtr IdentityInterface)
{
	UE_LOG_AB(Warning, TEXT("Lobby connection closed. Reason '%s' Code : '%d'"), *Reason, StatusCode);

	if (!IdentityInterface.IsValid())
	{
		UE_LOG_AB(Warning, TEXT("Error due to either IdentityInterface or ChatInteface is invalid"));
		return;
	}

	const TSharedPtr<const FUniqueNetId> UserIdPtr = IdentityInterface->GetUniquePlayerId(InLocalUserNum);
	TSharedPtr<FUserOnlineAccount> UserAccount;

	if (UserIdPtr.IsValid())
	{
		const FUniqueNetId& UserId = UserIdPtr.ToSharedRef().Get();
		UserAccount = IdentityInterface->GetUserAccount(UserId);
	}

	if (UserAccount.IsValid())
	{
		const TSharedPtr<FUserOnlineAccountAccelByte> UserAccountAccelByte = StaticCastSharedPtr<FUserOnlineAccountAccelByte>(UserAccount);
		UserAccountAccelByte->SetConnectedToChat(false);
	}
}

void FOnlineAsyncTaskAccelByteConnectChat::UnbindDelegates()
{
	OnChatConnectSuccessDelegate.Unbind();
	OnChatConnectErrorDelegate.Unbind();
	OnChatDisconnectedNotifDelegate.Unbind();
}
