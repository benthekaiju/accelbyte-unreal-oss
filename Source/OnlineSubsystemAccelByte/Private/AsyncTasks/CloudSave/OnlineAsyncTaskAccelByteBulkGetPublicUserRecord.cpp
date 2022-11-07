// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteBulkGetPublicUserRecord.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineCloudSaveInterfaceAccelByte.h"

FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord::FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord(FOnlineSubsystemAccelByte* const InABInterface, const FUniqueNetId& InLocalUserId, const FString& InKey, const TArray<FString>& InUserIds)
	: FOnlineAsyncTaskAccelByte(InABInterface)
	, Key(InKey)
	, UserIds(InUserIds)
{
	UserId = StaticCastSharedRef<const FUniqueNetIdAccelByteUser>(InLocalUserId.AsShared());
}

void FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord::Initialize()
{
	Super::Initialize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Getting bulk user record, UserId: %s"), *UserId->ToDebugString());

	OnBulkGetPublicUserRecordSuccessDelegate = TDelegateUtils<THandler<FListAccelByteModelsUserRecord>>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord::OnBulkGetPublicUserRecordSuccess);
	OnBulkGetPublicUserRecordErrorDelegate = TDelegateUtils<FErrorHandler>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord::OnBulkGetPublicUserRecordError);
	ApiClient->CloudSave.BulkGetPublicUserRecord(Key, UserIds, OnBulkGetPublicUserRecordSuccessDelegate, OnBulkGetPublicUserRecordErrorDelegate);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord::TriggerDelegates()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	const FOnlineCloudSaveAccelBytePtr CloudSaveInterface = Subsystem->GetCloudSaveInterface();
	if (CloudSaveInterface.IsValid())
	{
		if (bWasSuccessful)
		{
			CloudSaveInterface->TriggerOnBulkGetPublicUserRecordCompletedDelegates(LocalUserNum, true, ListUserRecord, TEXT(""));
		}
		else
		{
			CloudSaveInterface->TriggerOnBulkGetPublicUserRecordCompletedDelegates(LocalUserNum, false, ListUserRecord, ErrorStr);
		}
	}
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord::OnBulkGetPublicUserRecordSuccess(const FListAccelByteModelsUserRecord& Result)
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));
	ListUserRecord = Result;
	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT("Request to get bulk public user record for user '%s' Success!"), *UserId->ToDebugString());
}

void FOnlineAsyncTaskAccelByteBulkGetPublicUserRecord::OnBulkGetPublicUserRecordError(int32 ErrorCode, const FString& ErrorMessage)
{
	ErrorStr = TEXT("request-failed-get-bulk-public-user-record-error");
	UE_LOG_AB(Warning, TEXT("Failed to get bulk public user record! Error Code: %d; Error Message: %s"), ErrorCode, *ErrorMessage);
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}