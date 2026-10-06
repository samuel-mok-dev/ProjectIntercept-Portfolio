// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "ShipDataBase.h"
#include "ShipBase.generated.h"

//Forward Declarations
class UStaticMeshComponent;
class UFighterPropulsionComponent;
class UFighterLandingGearComponent;
class UCountermeasureComponent;
class UBoxComponent;
class USceneComponent;
class UCameraComponent;
class USpringArmComponent;
class UShipDataAsset;
class ULaserWeaponComponent;
class ALaserProjectile;
class UMissileWeaponComponent;
class AMissileProjectile;
class UHealthComponent;
class UTargetingComponent;
class UShipSensingComponent;
class UShipSteeringComponent;
class UAudioComponent;
class UNiagaraSystem;
class USoundBase;
class ASkirmishManager;
enum class ESecondaryWeaponMode : uint8;

USTRUCT()
struct FFlightInput
{
	GENERATED_BODY()

	UPROPERTY()
	float Throttle = 0.0f;
	UPROPERTY()
	float Pitch = 0.0f;
	UPROPERTY()
	float Yaw = 0.0f;
	UPROPERTY()
	float Roll = 0.0f;
};

UCLASS() 
class SPACEACE_API AShipBase : public APawn, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
    void SetLaunchLeader(AShipBase* Leader) { LaunchLeader=Leader; }
    void SetMissionStaged(bool bStaged);
    TWeakObjectPtr<AShipBase> LaunchLeader;
    TArray<TWeakObjectPtr<UActorComponent>> MissionPausedComponents;
    UPROPERTY(ReplicatedUsing=OnRep_CatapultStatus, BlueprintReadOnly, Category="Catapult") bool bOnCatapult = false;
    UFUNCTION() void OnRep_CatapultStatus();
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Catapult") bool bCatapultLaunching = false;
    void BeginCatapult(const FTransform& Start, const FVector& Exit);
    void UpdateCatapult(float DeltaTime);
    static float CatapultDistance(float Time, float Distance, float ExitSpeed);
    static float CatapultSpeed(float Time, float Distance, float ExitSpeed);
    float CatapultHold = 0.f;
    UPROPERTY(Replicated) float CatapultWait = 0.f;
    float CatapultElapsed = 0.f;
    UPROPERTY(Replicated) FVector CatapultStart = FVector::ZeroVector;
    UPROPERTY(Replicated) FVector CatapultExit = FVector::ZeroVector;
    UPROPERTY(Replicated) double CatapultStartedAt = 0;
	// Sets default values for this pawn's properties
	AShipBase();

	virtual void GetOwnedGameplayTags(
		FGameplayTagContainer& TagContainer
	) const override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UFighterLandingGearComponent> LandingGear;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCountermeasureComponent> Countermeasures;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UFighterPropulsionComponent> Propulsion;
	// Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> TargetPointComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")	
	TObjectPtr<USceneComponent> CannonLComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> CannonRComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MissileLComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MissileRComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULaserWeaponComponent> LaserWeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMissileWeaponComponent> MissileWeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> EngineLEffectComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> EngineREffectComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* EngineAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UShipSensingComponent> SensingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UShipSteeringComponent> SteeringComponent;

	
protected:
	// Ship Data
	UPROPERTY(ReplicatedUsing=OnRep_ShipData, EditAnywhere, BlueprintReadOnly, Category = "Ship Data")
	TObjectPtr<UShipDataAsset> ShipData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FLight")
	float CurrentSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float MaxSpeed = 7000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float MinSpeed = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float Acceleration = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float BasePitchTorque = 6.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float BaseYawTorque = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float BaseRollTorque = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float BaseBankTurnStrength = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float PitchSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float YawSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float RollSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	float BankTurnStrength = 0.0f;

	// Targeting System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FGameplayTag TeamTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FGameplayTag EnemyTeamTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting")
	TObjectPtr<AActor> CurrentTarget;

	// Missile Lock-On System
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	float MinimumLockOnDotProduct = 0.95f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting")
	bool bIsLockedOn = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting")
	float LockOnTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> DeathExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> DeathExplosionSound;

	TWeakObjectPtr<ASkirmishManager> OwningSkirmishManager = nullptr;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;

	void HandleDeath();

private:
	// Inputs from Player Controller
	FFlightInput FlightInput;

	float EnginePitchVariation = 1.0f;

    UFUNCTION(Server, Reliable)
    void ServerSetLaserFiring(bool bFiring);
    UFUNCTION(Server, Reliable)
    void ServerFireMissile();
    UFUNCTION(Server, Reliable)
    void ServerSwitchTarget();
    UFUNCTION(Server, Reliable)
    void ServerSwitchSecondaryWeapon();
    UFUNCTION()
    void OnRep_DeathHandled();
    UFUNCTION()
    void OnRep_ShipData();

    UPROPERTY(ReplicatedUsing=OnRep_DeathHandled)
    bool bDeathHandled = false;
    UPROPERTY(Replicated)
	bool bCombatEnabled = true;

	UPROPERTY(
			Replicated, VisibleInstanceOnly,
			BlueprintReadOnly,
			Category = "Combat",
			meta = (AllowPrivateAccess = "true")
		)
		bool bPreMatchFlight = false;

	// Targeting system
	TArray<AActor*> SortedTargets;
	int32 CurrentTargetIndex = -1;

	TSet<TWeakObjectPtr<AMissileProjectile>> IncomingMissiles;

	// Debug counter for velocity logging
	int32 DebugFrameCount = 0;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser
	) override;
	
	void SetShipData(UShipDataAsset* NewShipData);

	UShipDataAsset* GetShipData() const
	{
		return ShipData;
	}
	
	void ApplyShipData();
	
	void SetThrottleInput(float Value);
	void SetPitchInput(float Value);
	void SetYawInput(float Value);
	void SetRollInput(float Value);

	void SetFlightInput(const FFlightInput& NewFlightInput);

	void ApplyRotationTorque();

	void UpdateCurrentSpeed(float DeltaTime);

	void UpdateLinearVelocity();

	void SetSkirmishManager(ASkirmishManager* NewManager);
    ASkirmishManager* GetSkirmishManager() const;
	void SetCombatEnabled(bool bEnabled);
	bool IsCombatEnabled() const;

	void FireSingleMissile();
	void SwitchSecondaryWeaponMode();

	void StartFiringLasers();
	void StopFiringLasers();

	UFUNCTION(BlueprintImplementableEvent, Category = "Camera")
	void HandleLookInput(float YawValue, float PitchValue);

	void RegisterIncomingMissile(AMissileProjectile* Missile);
	void UnregisterIncomingMissile(AMissileProjectile* Missile);
	bool HasIncomingMissile() const;
	int32 GetIncomingMissileCount() const;
	float GetClosestIncomingMissileDistance() const;

	AActor* GetCurrentTarget() const;
	void SwitchTarget();
	void RefreshTargets();

	void SteerTowardsLocation(const FVector& WorldLocation);
	void SteerTowardsDirection(const FVector& WorldDirection);
	void StopAIControl();

	float GetLaserWeaponRange() const;
	float GetLaserProjectileSpeed() const;
	float GetMissileWeaponRange() const;
	bool HasLoadedMissile() const;
	int32 GetSpecialWeaponAmmo() const;
	ESecondaryWeaponMode GetSecondaryWeaponMode() const;
	bool IsCurrentSecondaryWeaponHoming() const;
	bool GetHasMissileInTheAir() const;
	
	bool GetIsLockedOn() const;

    UFUNCTION(BlueprintPure, Category = "Targeting")
    float GetLockOnProgress() const;
	bool IsDead() const;
	bool IsHostileTo(const AShipBase* OtherShip) const;
	void SetTeamID(int32 NewTeamID) { RuntimeTeamID = NewTeamID; }
	int32 GetTeamID() const { return RuntimeTeamID; }

	FVector GetShipForwardVector() const;
	FVector GetShipRightVector() const;
	FVector GetShipUpVector() const;
	FVector GetShipAngularVelocity() const;

	UShipSensingComponent* GetSensingComponent() const;

	float GetCurrentSpeed() const;
	float GetMaximumSpeed() const;
	float GetThrottleInput() const;

	float GetCurrentHealth() const;
	float GetMaximumHealth() const;
	float GetHealthPercentage() const;

	const FVector& GetFirstPersonCameraOffset() const;

	void UpdateEngineAudio();

	void ResetShip();
	void ExitAreaViaHyperspace();

	UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
	void PlayHyperspaceExitEffect();

	UPROPERTY(Replicated)
	int32 RuntimeTeamID = INDEX_NONE;

	void SetPreMatchFlight(bool bEnabled);

	bool IsInPreMatchFlight() const
	{
		return bPreMatchFlight;
	}
};
