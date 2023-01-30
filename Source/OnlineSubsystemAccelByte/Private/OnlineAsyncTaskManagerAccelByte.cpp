// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskManagerAccelByte.h"
#include "OnlineSubsystemAccelByte.h"

FOnlineAsyncTaskManagerAccelByte::FOnlineAsyncTaskManagerAccelByte(FOnlineSubsystemAccelByte* ParentSubsystem)
	: AccelByteSubsystem(ParentSubsystem)
{
}

void FOnlineAsyncTaskManagerAccelByte::OnlineTick()
{
	check(AccelByteSubsystem);
	check(FPlatformTLS::GetCurrentThreadId() == OnlineThreadId);
}

void FOnlineAsyncTaskManagerAccelByte::CheckMaxParallelTasks()
{
#if !UE_BUILD_SHIPPING
	if (MaxParallelTasks == ParallelTasks.Num())
	{
		UE_LOG(LogAccelByteOSS, Warning, TEXT("The number of Parallel Tasks has reached it cap: %d, Please put some delay between each tasks."), MaxParallelTasks);
	}
#endif
}
