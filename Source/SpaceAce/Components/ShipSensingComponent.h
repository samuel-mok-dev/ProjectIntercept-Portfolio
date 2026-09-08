// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplaytagContainer.h"
#include "ShipSensingComponent.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct FPerceivedContact
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	FVector LastKnownLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector LastKnownVelocity = FVector::ZeroVector;

	UPROPERTY()
	float LastSeenTime = 0.0f;

	UPROPERTY()
	bool bCurrentlyVisible = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPACEACE_API UShipSensingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShipSensingComponent();

	void Configure(const FGameplayTag& NewEnemyTeamTag);

	const TArray<FPerceivedContact>&
		GetPerceivedContacts() const;

	UFUNCTION(BLueprintPure, Category = "AI Debug")
	int32 GetRememberedContactCount() const;

	UFUNCTION(BlueprintPure, Category = "AI Debug")
	int32 GetVisibleContactCount() const;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateSenses();

	FPerceivedContact* FindContact(AActor* Actor);

	void UpdateOrCreateContact(AActor* Actor);

	void ForgetExpiredContacts();

	bool IsEnemyCombatant(AActor* Candidate) const;

	bool IsInsideFieldOfView(AActor* Candidate) const;

	UPROPERTY(EditAnywhere, Category = "Senses")
	float SensorRange = 300000.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Senses",
		meta = (ClampMin = "-1.0", ClampMax = "1.0")
	)
	float MinimumSightDotProduct = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Senses")
	float SenseUpdateInterval = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Senses")
	float ContactMemoryDuration = 5.0f;

	UPROPERTY()
	FGameplayTag EnemyTeamTag;

	TArray<FPerceivedContact> PerceivedContacts;

	FTimerHandle SenseUpdateTimerHandle;
};