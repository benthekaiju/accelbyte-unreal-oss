﻿// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteUpdateMemberStatus.h"
#include "GameServerApi/AccelByteServerSessionApi.h"
#include "Core/AccelByteRegistry.h"

FOnlineAsyncTaskAccelByteUpdateMemberStatus::FOnlineAsyncTaskAccelByteUpdateMemberStatus(FOnlineSubsystemAccelByte* const InABInterface, FName InSessionName, const FUniqueNetId& InPlayerId, const EAccelByteV2SessionMemberStatus& InStatus, const FOnSessionMemberStatusUpdateComplete& InDelegate)
	: FOnlineAsyncTaskAccelByte(InABInterface, ASYNC_TASK_FLAG_BIT(EAccelByteAsyncTaskFlags::ServerTask))
	, SessionName(InSessionName)
	, PlayerId(FUniqueNetIdAccelByteUser::CastChecked(InPlayerId))
	, Status(InStatus)
	, Delegate(InDelegate)
{
}

void FOnlineAsyncTaskAccelByteUpdateMemberStatus::Initialize()
{
	Super::Initialize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("SessionName: %s; PlayerId: %s; Status: %s"), *SessionName.ToString(), *PlayerId->ToDebugString(), *StaticEnum<EAccelByteV2SessionMemberStatus>()->GetNameStringByValue(static_cast<int64>(Status)));

	FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
	AB_ASYNC_TASK_ENSURE(FOnlineSessionV2AccelByte::GetFromSubsystem(Subsystem, SessionInterface), "Could not get session interface instance from subsystem!");

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
	AB_ASYNC_TASK_ENSURE(Session != nullptr, "Failed to find session locally with name of '%s'!", *SessionName.ToString());

	TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
	AB_ASYNC_TASK_ENSURE(SessionInfo.IsValid(), "Failed to get session info from local session named '%s'!", *SessionName.ToString());
	AB_ASYNC_TASK_ENSURE(SessionInfo->ContainsMember(PlayerId.ToSharedRef().Get()), "Failed to find member with ID '%s' in session '%s'!", *PlayerId->ToDebugString(), *SessionName.ToString());

	AB_ASYNC_TASK_DEFINE_SDK_DELEGATES(FOnlineAsyncTaskAccelByteUpdateMemberStatus, UpdateMemberStatus, FVoidHandler);
	FRegistry::ServerSession.UpdateMemberStatus(Session->GetSessionIdStr(), PlayerId->GetAccelByteId(), Status, OnUpdateMemberStatusSuccessDelegate, OnUpdateMemberStatusErrorDelegate);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteUpdateMemberStatus::Finalize()
{
	Super::Finalize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));
	
	if (bWasSuccessful)
	{
		// Grab our session interface, find the local session instance
		FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
		check(FOnlineSessionV2AccelByte::GetFromSubsystem(Subsystem, SessionInterface));

		FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
		check(Session != nullptr);

		// Grab our session information instance, and try and find the member that we just updated status for
		TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
		check(SessionInfo.IsValid());

		FAccelByteModelsV2SessionUser* FoundMember = nullptr;
		check(SessionInfo->FindMember(PlayerId.ToSharedRef().Get(), FoundMember));

		// Finally, update their status to reflect the backend status set
		FoundMember->Status = Status;

		// #NOTE Currently since servers don't get updates from DSHub relating to session updates, we have to manually
		// increment the version after we update a member's status. This can be replaced once server's get updates from
		// session service.
		TSharedPtr<FAccelByteModelsV2BaseSession> BackendData = SessionInfo->GetBackendSessionData();
		check(BackendData.IsValid());

		BackendData->Version++;

	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteUpdateMemberStatus::TriggerDelegates()
{
	Super::TriggerDelegates();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	Delegate.ExecuteIfBound(bWasSuccessful);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteUpdateMemberStatus::OnUpdateMemberStatusSuccess()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteUpdateMemberStatus::OnUpdateMemberStatusError(int32 ErrorCode, const FString& ErrorMessage)
{
	AB_ASYNC_TASK_REQUEST_FAILED("Failed to update status of member '%s' on backend!", ErrorCode, ErrorMessage, *PlayerId->ToDebugString());
}
