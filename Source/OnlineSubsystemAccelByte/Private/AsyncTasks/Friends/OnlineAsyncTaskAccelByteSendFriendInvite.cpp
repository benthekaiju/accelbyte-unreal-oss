// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteSendFriendInvite.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineFriendsInterfaceAccelByte.h"
#include "Api/AccelByteLobbyApi.h"

FOnlineAsyncTaskAccelByteSendFriendInvite::FOnlineAsyncTaskAccelByteSendFriendInvite(FOnlineSubsystemAccelByte* const InABInterface, int32 InLocalUserNum, const FUniqueNetId& InFriendId, const FString& InListName, const FOnSendInviteComplete& InDelegate)
	: FOnlineAsyncTaskAccelByte(InABInterface)
	, FriendId(FUniqueNetIdAccelByteUser::CastChecked(InFriendId))
	, ListName(InListName)
	, Delegate(InDelegate)
{
	LocalUserNum = InLocalUserNum;
}

FOnlineAsyncTaskAccelByteSendFriendInvite::FOnlineAsyncTaskAccelByteSendFriendInvite(FOnlineSubsystemAccelByte* const InABInterface, int32 InLocalUserNum, const FString& InFriendCode, const FString& InListName, const FOnSendInviteComplete& InDelegate)
	: FOnlineAsyncTaskAccelByte(InABInterface)
	, FriendId(FUniqueNetIdAccelByteUser::Invalid())
	, FriendCode(InFriendCode)
	, ListName(InListName)
	, Delegate(InDelegate)
{
	LocalUserNum = InLocalUserNum;
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::Initialize()
{
	Super::Initialize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("LocalUserNum: %d"), LocalUserNum);

	// If we have the ID of the user that we want to friend, send that request off
	if (FriendId->IsValid())
	{
		QueryInvitedFriend(FriendId->GetAccelByteId());
	}
	// If we have no friend ID but we do have a friend code, then send a request with that friend code
	else if (!FriendCode.IsEmpty())
	{
		// Friend codes *must* be all uppercase, since it may be faster for the user to input the code as all lower case, just
		// convert the friend code to an all uppercase string before performing any operations with it
		FriendCode.ToUpperInline();

		const THandler<FAccelByteModelsPublicUserProfileInfo> OnGetUserByFriendCodeSuccessDelegate = TDelegateUtils<THandler<FAccelByteModelsPublicUserProfileInfo>>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteSendFriendInvite::OnGetUserByFriendCodeSuccess);
		const FCustomErrorHandler OnGetUserByFriendCodeErrorDelegate = TDelegateUtils<FCustomErrorHandler>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteSendFriendInvite::OnGetUserByFriendCodeError);
		ApiClient->UserProfile.GetUserProfilePublicInfoByPublicId(FriendCode, OnGetUserByFriendCodeSuccessDelegate, OnGetUserByFriendCodeErrorDelegate);
	}
	else
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("ID of the user you wish to friend or a friend code must be specified to send a request!"));
		CompleteTask(EAccelByteAsyncTaskCompleteState::InvalidState);
		return;
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::Finalize()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	if (bWasSuccessful)
	{
		const TSharedPtr<FOnlineFriendsAccelByte, ESPMode::ThreadSafe> FriendInterface = StaticCastSharedPtr<FOnlineFriendsAccelByte>(Subsystem->GetFriendsInterface());
		FriendInterface->AddFriendToList(LocalUserNum, InvitedFriend);
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::TriggerDelegates()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, ((bWasSuccessful) ? InvitedFriend->GetUserId().Get() : FriendId.Get()), ListName, ErrorStr);
	if (bWasSuccessful)
	{
		const TSharedPtr<FOnlineFriendsAccelByte, ESPMode::ThreadSafe> FriendInterface = StaticCastSharedPtr<FOnlineFriendsAccelByte>(Subsystem->GetFriendsInterface());
		FriendInterface->TriggerOnOutgoingInviteSentDelegates(LocalUserNum);
	}
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::OnGetUserByFriendCodeSuccess(const FAccelByteModelsPublicUserProfileInfo& Result)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("FriendCode: %s, UserId: %s"), *FriendCode, *Result.UserId);

	const TSharedPtr<FOnlineFriendsAccelByte, ESPMode::ThreadSafe> FriendInterface = StaticCastSharedPtr<FOnlineFriendsAccelByte>(Subsystem->GetFriendsInterface());
	const TSharedPtr<FOnlineIdentityAccelByte, ESPMode::ThreadSafe> IdentityInt = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());

	if (!FriendInterface || !IdentityInt)
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Unable to send friend invited friend interface or indentity interface is invalid"));
		CompleteTask(EAccelByteAsyncTaskCompleteState::InvalidState);
		return;
	}

	TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
	TSharedPtr<const FUniqueNetIdAccelByteUser> FriendUserId = FUniqueNetIdAccelByteUser::Create(Result.UserId);

	if (!LocalUserId || !FriendUserId)
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Unable to send friend invited invalid user id"));
		CompleteTask(EAccelByteAsyncTaskCompleteState::InvalidState);
		return;
	}

	if (FriendInterface->IsPlayerBlocked(*FriendUserId, *LocalUserId))
	{
		ErrorStr = TEXT("friend-request-requester-blocked");
		CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
		return;
	}

	QueryInvitedFriend(Result.UserId);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT("Sent off request to send a friend invite to user %s"), *Result.UserId);
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::OnGetUserByFriendCodeError(int32 ErrorCode, const FString& ErrorMessage, const FJsonObject& ErrorObject)
{
	ErrorStr = TEXT("friend-request-invalid-code");
	UE_LOG_AB(Warning, TEXT("Failed to get user by friend code %s to send a friend invite!"), *FriendCode);
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::QueryInvitedFriend(const FString& InFriendId)
{
	FOnlineUserCacheAccelBytePtr UserStore = Subsystem->GetUserCache();
	if (!UserStore.IsValid())
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Could not query invited friend '%s' as our user store instance is invalid!"), *InFriendId);
		CompleteTask(EAccelByteAsyncTaskCompleteState::InvalidState);
		return;
	}

	FOnQueryUsersComplete OnQueryInvitedFriendCompleteDelegate = TDelegateUtils<FOnQueryUsersComplete>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteSendFriendInvite::OnQueryInvitedFriendComplete);
	UserStore->QueryUsersByAccelByteIds(LocalUserNum, { InFriendId }, OnQueryInvitedFriendCompleteDelegate, true);
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::OnQueryInvitedFriendComplete(bool bIsSuccessful, TArray<TSharedRef<FAccelByteUserInfo>> UsersQueried)
{
	if (bIsSuccessful && UsersQueried.IsValidIndex(0))
	{
		TSharedRef<FAccelByteUserInfo> User = UsersQueried[0];

		// Create the friend instance first, and then try and send the invite, this will only be added to the list if the
		// invite request successfully sends. Use the queried ID instead of the AccelByte ID, as that will have their
		// platform ID filled out
		InvitedFriend = MakeShared<FOnlineFriendAccelByte>(User->DisplayName, User->Id.ToSharedRef(), EInviteStatus::PendingOutbound);

		// Send the actual request to send the friend request
		AccelByte::Api::Lobby::FRequestFriendsResponse OnRequestFriendResponseDelegate = TDelegateUtils<AccelByte::Api::Lobby::FRequestFriendsResponse>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteSendFriendInvite::OnRequestFriendResponse);
		ApiClient->Lobby.SetRequestFriendsResponseDelegate(OnRequestFriendResponseDelegate);
		ApiClient->Lobby.RequestFriend(User->Id->GetAccelByteId());
	}
	else
	{
		ErrorStr = TEXT("friend-request-failed");
		UE_LOG_AB(Warning, TEXT("Failed to get query information on user '%s' for friend invite!"), *FriendId->ToDebugString());
		CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
	}
}

void FOnlineAsyncTaskAccelByteSendFriendInvite::OnRequestFriendResponse(const FAccelByteModelsRequestFriendsResponse& Result)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("FriendId: %s"), *InvitedFriend->GetUserId()->ToDebugString());

	if (Result.Code != TEXT("0"))
	{
		ErrorStr = TEXT("friend-request-failed");
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Failed to send friend request to user %s! Error code: %s"), *InvitedFriend->GetUserId()->ToDebugString(), *Result.Code);
		CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
	}
	else
	{
		CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}
