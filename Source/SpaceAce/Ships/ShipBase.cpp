// Fill out your copyright notice in the Description page of Project Settings.


#include "ShipBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "HealthComponent.h"
#include "LaserWeaponComponent.h"
#include "MissileWeaponComponent.h"
#include "TargetingComponent.h"
#include "ShipSensingComponent.h"
#include "ShipSteeringComponent.h"
#include "LaserProjectile.h"
#include "MissileProjectile.h"
#include "GameplayTagAssetInterface.h"
#include "EngineUtils.h"
#include "CombatantSubsystem.h"
#include "ShipAIController.h"
#include "Components/AudioComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "SkirmishManager.h"

// Sets default values
AShipBase::AShipBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	SetRootComponent(ShipMesh);
	ShipMesh->SetSimulatePhysics(true);
	ShipMesh->SetEnableGravity(false);
	ShipMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	ShipMesh->SetLinearDamping(0.0f);
	ShipMesh->SetAngularDamping(4.0f);

	TargetPointComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TargetPoint"));
	TargetPointComponent->SetupAttachment(ShipMesh);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(ShipMesh);
	CollisionBox->SetBoxExtent(FVector(10.0f, 10.0f, 10.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Block);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	LaserWeaponComponent = CreateDefaultSubobject<ULaserWeaponComponent>(TEXT("LaserWeaponComponent"));
	MissileWeaponComponent = CreateDefaultSubobject<UMissileWeaponComponent>(TEXT("MissileWeaponComponent"));

	TargetingComponent = CreateDefaultSubobject <UTargetingComponent>(TEXT("TargetingComponent"));

	SensingComponent = CreateDefaultSubobject<UShipSensingComponent>(TEXT("SensingComponent"));

	SteeringComponent = CreateDefaultSubobject<UShipSteeringComponent>(TEXT("SteeringComponent"));

	CannonLComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CannonL"));
	CannonLComponent->SetupAttachment(ShipMesh);
	CannonLComponent->SetRelativeLocation(FVector(100.0f, -50.0f, 0.0f));

	CannonRComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CannonR"));
	CannonRComponent->SetupAttachment(ShipMesh);
	CannonRComponent->SetRelativeLocation(FVector(100.0f, 50.0f, 0.0f));

	MissileLComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MissileL"));
	MissileLComponent->SetupAttachment(ShipMesh);

	MissileRComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MissileR"));
	MissileRComponent->SetupAttachment(ShipMesh);

	EngineLEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EngineEffectL"));
	EngineLEffectComponent->SetupAttachment(ShipMesh);

	EngineREffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EngineEffectR"));
	EngineREffectComponent->SetupAttachment(ShipMesh);

	EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(ShipMesh);
	EngineAudio->bAutoActivate = false;


}

// Called when the game starts or when spawned
void AShipBase::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UCombatantSubsystem* CombatantSubsystem =
			World->GetSubsystem<UCombatantSubsystem>())
		{
			CombatantSubsystem->RegisterCombatant(this);
		}
	}

	ApplyShipData();

	if (ShipMesh)
	{
		FVector InitialVelocity = GetActorForwardVector() * CurrentSpeed;
		ShipMesh->SetPhysicsLinearVelocity(InitialVelocity);
	}
}

void AShipBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UCombatantSubsystem* CombatantSubsystem =
			World->GetSubsystem<UCombatantSubsystem>())
		{
			CombatantSubsystem->UnregisterCombatant(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void AShipBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bCombatEnabled)
	{
		return;
	}

	if (bPreMatchFlight)
	{
		if (ShipMesh)
		{
			ShipMesh->SetPhysicsLinearVelocity(
				GetActorForwardVector() * CurrentSpeed
			);

			ShipMesh->SetPhysicsAngularVelocityInRadians(
				FVector::ZeroVector
			);
		}

		UpdateEngineAudio();
		return;
	}

	UpdateCurrentSpeed(DeltaTime);
	UpdateLinearVelocity();
	ApplyRotationTorque();
	UpdateEngineAudio();
	
	if (TargetingComponent)
	{
		TargetingComponent->UpdateLockOn(DeltaTime);
	}

}

void AShipBase::SetShipData(UShipDataAsset* NewShipData)
{
	ShipData = NewShipData;
}

// Apply ship data to the controlled pawn
void AShipBase::ApplyShipData()
{
	// Ensure ShipData is valid before applying it
	if (!ShipData)
	{
		UE_LOG(LogTemp, 
			Warning, 
			TEXT("ShipData is null. Cannot apply ship data.")
		);
		return;
	}

	// Set the ship's static mesh from the ShipData asset
	if (ShipMesh && ShipData->ShipMesh)
	{
		ShipMesh->SetStaticMesh(ShipData->ShipMesh);
	}

	// Initialize HealthComponent with MaxHealth from ShipData
	HealthComponent->InitializeHealth(ShipData->MaxHealth);

	// Apply ship data values to the ship's properties

	CurrentSpeed = ShipData->CurrentSpeed;
	MaxSpeed = ShipData->MaxSpeed;
	MinSpeed = ShipData->MinSpeed;
	Acceleration = ShipData->Acceleration;

	PitchSpeed = BasePitchTorque * ShipData->PitchTorqueMultiplier;
	YawSpeed = BaseYawTorque * ShipData->YawTorqueMultiplier;
	RollSpeed = BaseRollTorque * ShipData->RollTorqueMultiplier;
	BankTurnStrength = BaseBankTurnStrength * ShipData->BankTurnMultiplier;

	// Set the relative locations and rotations of the cannon and missile 
	// components based on ShipData
	CannonLComponent->SetRelativeLocation(ShipData->CannonOffsetL);
	CannonRComponent->SetRelativeLocation(ShipData->CannonOffsetR);

	CannonLComponent->SetRelativeRotation(FRotator(0.0f, +ShipData->CannonRotationOffset.Y, 0.0f));
	CannonRComponent->SetRelativeRotation(FRotator(0.0f, -ShipData->CannonRotationOffset.Y, 0.0f));

	TArray<USceneComponent*> ActiveCannonMuzzles;

	if (CannonLComponent)
	{
		ActiveCannonMuzzles.Add(CannonLComponent);
	}

	if (CannonRComponent)
	{
		ActiveCannonMuzzles.Add(CannonRComponent);
	}

	if (LaserWeaponComponent)
	{
		LaserWeaponComponent->Configure(
			ActiveCannonMuzzles,
			ShipData->LaserDamageMultiplier,
			ShipData->LaserSpeedMultiplier,
			ShipData->GunFireRate,
			ShipData->LaserPoolSize,
			ShipData->LaserProjectileClass,
			ShipData->LaserMaterial
		);

		LaserWeaponComponent->InitializeLaserPool();
	}

	MissileLComponent->SetRelativeLocation(ShipData->MissileOffsetL);
	MissileRComponent->SetRelativeLocation(ShipData->MissileOffsetR);

	TArray<USceneComponent*> MissileHardpoints;

	if (MissileLComponent)
	{
		MissileHardpoints.Add(MissileLComponent);
	}
	if (MissileRComponent)
	{
		MissileHardpoints.Add(MissileRComponent);
	}

	if (MissileWeaponComponent)
	{
		MissileWeaponComponent->Configure(MissileHardpoints);
		MissileWeaponComponent->InitializeMissilePool();
	}

	if (SensingComponent)
	{
		SensingComponent->Configure(EnemyTeamTag);
	}

	if (TargetingComponent)
	{
		TargetingComponent->Configure(EnemyTeamTag);
		TargetingComponent->AcquireTargets();
	}

	if (!ShipData || !EngineAudio)
	{
		return;
	}

	if (ShipData->HasTwoEngines)
	{
		EngineLEffectComponent->SetAsset(ShipData->EngineEffect);
		EngineLEffectComponent->SetRelativeLocation(ShipData->EngineLEffectOffset);

		EngineREffectComponent->SetAsset(ShipData->EngineEffect);
		EngineREffectComponent->SetRelativeLocation(ShipData->EngineREffectOffset);
	}
	else
	{
		EngineLEffectComponent->SetAsset(ShipData->EngineEffect);
		EngineLEffectComponent->SetRelativeLocation(ShipData->EngineLEffectOffset);

		if (EngineREffectComponent)
		{
			EngineREffectComponent->Deactivate();
			EngineREffectComponent->SetVisibility(false);
		}
	} 

	EngineAudio->SetSound(ShipData->EngineSound);

	if (!EngineAudio->IsPlaying())
	{
		EngineAudio->Play();
	}
}

// Called to bind functionality to input
void AShipBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

// Setter functions for input values
void AShipBase::SetThrottleInput(float Value)
{
	ThrottleInput = bCombatEnabled && !bPreMatchFlight ? Value : 0.0f;
}

void AShipBase::SetPitchInput(float Value)
{
	PitchInput = bCombatEnabled && !bPreMatchFlight ? Value : 0.0f;
}

void AShipBase::SetYawInput(float Value)
{
	YawInput = bCombatEnabled && !bPreMatchFlight ? Value : 0.0f;
}

void AShipBase::SetRollInput(float Value)
{
	RollInput = bCombatEnabled && !bPreMatchFlight ? Value : 0.0f;
}

// Update the current speed based on throttle input
void AShipBase::UpdateCurrentSpeed(float DeltaTime)
{
	if (!ShipMesh)
	{
		return;
	}

	float OldSpeed = CurrentSpeed;
	CurrentSpeed += ThrottleInput * Acceleration * DeltaTime;
	CurrentSpeed = FMath::Clamp(CurrentSpeed, MinSpeed, MaxSpeed);
}

// Set the linear velocity of the ship based on the throttle input
void AShipBase::UpdateLinearVelocity()
{
	if (!ShipMesh)
	{
		return;
	}

	const FVector MeshForward = ShipMesh->GetForwardVector();
	const FVector ActorForward = GetActorForwardVector();
	const FVector Velocity = MeshForward * CurrentSpeed;

	ShipMesh->SetPhysicsLinearVelocity(Velocity);

	// Log first 3 frames to see what's happening
	if (DebugFrameCount < 3)
	{
		DebugFrameCount++;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s Tick %d: UpdateLinearVelocity | CurrentSpeed=%.1f | MeshForward=%s | ActorForward=%s | SetVelocity=%s"),
			*GetName(),
			DebugFrameCount,
			CurrentSpeed,
			*MeshForward.ToString(),
			*ActorForward.ToString(),
			*Velocity.ToString()
		);
	}
}

// Set torque values for pitch, yaw, and roll based on input values
void AShipBase::ApplyRotationTorque()
{
	// Ensure the ShipMesh is valid before applying torque
	if (!ShipMesh)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("ShipMesh is not valid. Cannot apply rotation torque.")
		);
		return;
	}

	// Calculate torque values based on input values and a rotation speed factor
	const FVector BankTorque = GetShipUpVector() * -RollInput * BankTurnStrength;
	
	const FVector PitchTorque = ShipMesh->GetRightVector() * PitchInput * PitchSpeed;
	const FVector YawTorque = ShipMesh->GetUpVector() * YawInput * YawSpeed + BankTorque;
	const FVector RollTorque = ShipMesh->GetForwardVector() * RollInput * RollSpeed;
	
	// Apply torque to the ship's mesh
	ShipMesh->AddTorqueInRadians(PitchTorque + YawTorque + RollTorque, NAME_None, true);

	/*UE_LOG(
		LogTemp,
		Warning,
		TEXT("Applied Rotation Torque: Pitch=%s, Yaw=%s, Roll=%s"),
		*PitchTorque.ToString(),
		*YawTorque.ToString(),
		*RollTorque.ToString()
	);*/
}

// Start continuous gun firing
void AShipBase::StartFiringLasers()
{
	if (bCombatEnabled && LaserWeaponComponent && !bPreMatchFlight)
	{
		LaserWeaponComponent->StartFiringLasers();
	}
}

// Stop continuous gun firing
void AShipBase::StopFiringLasers()
{
	if (LaserWeaponComponent)
	{
		LaserWeaponComponent->StopFiringLasers();
	}
}

// Fire ze missiles
void AShipBase::FireSingleMissile()
{
	if (!bCombatEnabled || !MissileWeaponComponent || !TargetingComponent || bPreMatchFlight)
	{
		return;
	}

	MissileWeaponComponent->FireMissile(
		TargetingComponent->GetCurrentTarget(),
		TargetingComponent->IsLockedOn()
	);
	
}

void AShipBase::SwitchSecondaryWeaponMode()
{
	if (MissileWeaponComponent)
	{
		MissileWeaponComponent->SwitchWeaponMode();
	}
}

void AShipBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.Reset();

	if (TeamTag.IsValid())
	{
		TagContainer.AddTag(TeamTag);
	}
}

void AShipBase::SwitchTarget()
{
	UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s: ShipBase received SwitchTarget"),
        *GetNameSafe(this)
    );
	
	if (TargetingComponent)
	{
		TargetingComponent->SwitchTarget();
	}

	else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s: TargetingComponent is null"),
            *GetNameSafe(this)
        );
    }
}

AActor* AShipBase::GetCurrentTarget() const
{
	return TargetingComponent 
		? TargetingComponent->GetCurrentTarget()
		: nullptr;
}

bool AShipBase::GetIsLockedOn() const
{
	return TargetingComponent
		? TargetingComponent->IsLockedOn()
		: false;
}

float AShipBase::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (!bCombatEnabled || bPreMatchFlight || !HealthComponent || DamageAmount <= 0.0f)
    {
        return 0.0f;
    }

    // Identify the ship responsible for the damage.
    AShipBase* Attacker = nullptr;

    if (EventInstigator)
    {
        Attacker = Cast<AShipBase>(EventInstigator->GetPawn());
    }

    if (!Attacker && DamageCauser)
    {
        Attacker = Cast<AShipBase>(DamageCauser->GetOwner());
    }

    const float HealthBefore = HealthComponent->GetCurrentHealth();

    HealthComponent->ApplyDamage(DamageAmount);

    const float HealthAfter = HealthComponent->GetCurrentHealth();
    const float ActualDamage = FMath::Max(0.0f, HealthBefore - HealthAfter);

    if (ASkirmishManager* Manager = OwningSkirmishManager.Get())
	{
		Manager->ReportDamage(
			this,
			Attacker,
			ActualDamage
		);
	}

    if (HealthComponent->IsDead())
	{
		if (ASkirmishManager* Manager = OwningSkirmishManager.Get())
		{
			Manager->ReportDeath(
				this,
				Attacker
			);
		}

		HandleDeath();
	}

    return ActualDamage;
}

void AShipBase::HandleDeath()
{
	if (bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;

	if (UWorld* World = GetWorld())
	{
		if (UCombatantSubsystem* CombatantSubsystem =
			World->GetSubsystem<UCombatantSubsystem>())
		{
			CombatantSubsystem->UnregisterCombatant(this);
		}
	}

	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);

	if (LaserWeaponComponent)
	{
		LaserWeaponComponent->StopFiringLasers();
		LaserWeaponComponent->Deactivate();
	}
	
	if (MissileWeaponComponent)
	{
		MissileWeaponComponent->Deactivate();
	}

	if (TargetingComponent)
	{
		TargetingComponent->Deactivate();
	}

	if (HealthComponent)
	{
		HealthComponent->Deactivate();
	}

	if (SensingComponent)
	{
		SensingComponent->Deactivate();
	}

	if (SteeringComponent)
	{
		SteeringComponent->StopSteering();
		SteeringComponent->Deactivate();
	}

	if (EngineAudio && EngineAudio->IsPlaying())
	{
		EngineAudio->Stop();
	}

	if (EngineLEffectComponent && EngineLEffectComponent->IsActive())
	{
		EngineLEffectComponent->Deactivate();
	}

	if (EngineREffectComponent && EngineREffectComponent->IsActive())
	{
		EngineREffectComponent->Deactivate();
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (PlayerController)
	{
		PlayerController->UnPossess();
	}

	if (DeathExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			DeathExplosionEffect,
			GetActorLocation(),
			GetActorRotation()
		);
	}

	if (DeathExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			DeathExplosionSound,
			GetActorLocation()
		);
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s has been deactivated"),
		*GetNameSafe(this)
	);
}

void AShipBase::ResetShip()
{
	bDeathHandled = false;
	bCombatEnabled = true;

	if (UWorld* World = GetWorld())
	{
		if (UCombatantSubsystem* CombatantSubsystem =
			World->GetSubsystem<UCombatantSubsystem>())
		{
			CombatantSubsystem->RegisterCombatant(this);
		}
	}

	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	SetActorHiddenInGame(false);
	IncomingMissiles.Empty();

	if (HealthComponent)
	{
		HealthComponent->Activate();
		HealthComponent->ResetHealth();
	}

	if (LaserWeaponComponent)
	{
		LaserWeaponComponent->Activate();
	}

	if (MissileWeaponComponent)
	{
		MissileWeaponComponent->Activate();
	}

	if (TargetingComponent)
	{
		TargetingComponent->Activate();
	}

	if (SensingComponent)
	{
		SensingComponent->Activate();
	}

	if (SteeringComponent)
	{
		SteeringComponent->Activate();
	}

	if (EngineAudio && !EngineAudio->IsPlaying())
	{
		EngineAudio->Play();
	}

	if (EngineLEffectComponent && !EngineLEffectComponent->IsActive())
	{
		EngineLEffectComponent->Activate();
	}

	if (EngineREffectComponent && !EngineREffectComponent->IsActive())
	{
		EngineREffectComponent->Activate();
	}

	if (ShipMesh)
	{
		FVector InitialVelocity = GetActorForwardVector() * CurrentSpeed;
		ShipMesh->SetPhysicsLinearVelocity(InitialVelocity);
		ShipMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}

	if (AShipAIController* AIController =
		Cast<AShipAIController>(GetController()))
	{
		AIController->ResetForRespawn();
	}
}

bool AShipBase::IsDead() const
{
	return HealthComponent ? HealthComponent->IsDead() : true;
}

FVector AShipBase::GetShipForwardVector() const
{
	return ShipMesh ? ShipMesh->GetForwardVector() : GetActorForwardVector();
}

FVector AShipBase::GetShipRightVector() const
{
	return ShipMesh ? ShipMesh->GetRightVector() : GetActorRightVector();
}

FVector AShipBase::GetShipUpVector() const
{
	return ShipMesh ? ShipMesh->GetUpVector() : GetActorUpVector();
}

FVector AShipBase::GetShipAngularVelocity() const
{
	return ShipMesh 
		? ShipMesh->GetPhysicsAngularVelocityInRadians() 
		: FVector::ZeroVector;
}

UShipSensingComponent* AShipBase::GetSensingComponent() const
{
	return SensingComponent;
}

void AShipBase::SteerTowardsLocation(const FVector& WorldLocation)
{
	if (bCombatEnabled && SteeringComponent)
	{
		SteeringComponent->SteerTowardsLocation(WorldLocation);
	}
}

void AShipBase::SteerTowardsDirection(const FVector& WorldDirection)
{
	if (bCombatEnabled && SteeringComponent)
	{
		SteeringComponent->SteerTowardsDirection(WorldDirection);
	}
}

void AShipBase::StopAIControl()
{
	if (SteeringComponent)
	{
		SteeringComponent->StopSteering();
	}

	SetThrottleInput(0.0f);
	StopFiringLasers();
}

float AShipBase::GetLaserWeaponRange() const
{
	return LaserWeaponComponent
		? LaserWeaponComponent->GetEffectiveRange()
		: 0.0f;
}

float AShipBase::GetLaserProjectileSpeed() const
{
	return LaserWeaponComponent
		? LaserWeaponComponent->GetProjectileSpeed()
		: 0.0f;
}

float AShipBase::GetMissileWeaponRange() const
{
	return MissileWeaponComponent
		? MissileWeaponComponent->GetEffectiveRange()
		: 0.0f;
}

bool AShipBase::HasLoadedMissile() const
{
	return MissileWeaponComponent
		? MissileWeaponComponent->HasLoadedMissile()
		: false;
}

int32 AShipBase::GetSpecialWeaponAmmo() const
{
	return MissileWeaponComponent
		? MissileWeaponComponent->GetSpecialWeaponAmmo()
		: 0;
}

ESecondaryWeaponMode AShipBase::GetSecondaryWeaponMode() const
{
	return MissileWeaponComponent
		? MissileWeaponComponent->GetWeaponMode()
		: ESecondaryWeaponMode::StandardMissile;
}

bool AShipBase::IsCurrentSecondaryWeaponHoming() const
{
	return MissileWeaponComponent && MissileWeaponComponent->IsCurrentWeaponHoming();
}

float AShipBase::GetCurrentSpeed() const
{
	return CurrentSpeed;
}

float AShipBase::GetMaximumSpeed() const
{
	return MaxSpeed;
}

float AShipBase::GetThrottleInput() const
{
	return ThrottleInput;
}

float AShipBase::GetCurrentHealth() const
{
	return HealthComponent ? HealthComponent->GetCurrentHealth() : 0.0f;
}

float AShipBase::GetMaximumHealth() const
{
	return HealthComponent ? HealthComponent->GetMaxHealth() : 0.0f;
}

float AShipBase::GetHealthPercentage() const
{
	const float MaxHealth = GetMaximumHealth();

	return MaxHealth > KINDA_SMALL_NUMBER
		? GetCurrentHealth() / MaxHealth
		: 0.0f;
}

bool AShipBase::IsHostileTo(
	const AShipBase* OtherShip
) const
{
	if (!OtherShip)
	{
		return false;
	}

	return
		EnemyTeamTag.IsValid() &&
		OtherShip->TeamTag.MatchesTagExact(EnemyTeamTag);
}

void AShipBase::RefreshTargets()
{
	if (!TargetingComponent)
	{
		return;
	}

	TargetingComponent->AcquireTargets();
}

void AShipBase::RegisterIncomingMissile(AMissileProjectile* Missile)
{
	if (!IsValid(Missile))
	{
		return;
	}

	IncomingMissiles.Add(Missile);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s: Incoming missile detected. Count = %d"),
		*GetNameSafe(this),
		IncomingMissiles.Num()
	);
}

void AShipBase::UnregisterIncomingMissile(AMissileProjectile* Missile)
{
	if (!IsValid(Missile))
	{
		return;
	}

	IncomingMissiles.Remove(Missile);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s: Incoming missile cleared. Count = %d"),
		*GetNameSafe(this),
		IncomingMissiles.Num()
	);
}

bool AShipBase::HasIncomingMissile() const
{
    for (
        const TWeakObjectPtr<AMissileProjectile>& Missile :
        IncomingMissiles
    )
    {
        if (Missile.IsValid())
        {
            return true;
        }
    }

    return false;
}

int32 AShipBase::GetIncomingMissileCount() const
{
    int32 ValidMissileCount = 0;

    for (
        const TWeakObjectPtr<AMissileProjectile>& Missile :
        IncomingMissiles
    )
    {
        if (Missile.IsValid())
        {
            ++ValidMissileCount;
        }
    }

    return ValidMissileCount;
}

float AShipBase::GetClosestIncomingMissileDistance() const
{
    float ClosestDistance = TNumericLimits<float>::Max();

    for (
        const TWeakObjectPtr<AMissileProjectile>& MissilePointer :
        IncomingMissiles
    )
    {
        const AMissileProjectile* Missile =
            MissilePointer.Get();

        if (!IsValid(Missile))
        {
            continue;
        }

        const float Distance =
            FVector::Distance(
                GetActorLocation(),
                Missile->GetActorLocation()
            );

        ClosestDistance =
            FMath::Min(
                ClosestDistance,
                Distance
            );
    }

    return ClosestDistance;
}

bool AShipBase::GetHasMissileInTheAir() const
{
	return MissileWeaponComponent
		? MissileWeaponComponent->HasMissileInTheAir()
		: false;
}

void AShipBase::UpdateEngineAudio()
{
    if (!ShipData || !EngineAudio)
    {
        return;
    }

    const float NormalizedSpeed =
    FMath::GetMappedRangeValueClamped(
        FVector2D(MinSpeed, MaxSpeed),
        FVector2D(0.0f, 1.0f),
        CurrentSpeed
    );

    const float Pitch = FMath::Lerp(
        ShipData->EngineIdlePitch,
        ShipData->EngineMaxPitch,
        NormalizedSpeed);

    const float Volume = FMath::Lerp(
        ShipData->EngineIdleVolume,
        ShipData->EngineMaxVolume,
        NormalizedSpeed);

    EngineAudio->SetPitchMultiplier(Pitch);
    EngineAudio->SetVolumeMultiplier(Volume);
}

const FVector& AShipBase::GetFirstPersonCameraOffset() const
{
	return ShipData ? ShipData->FirstPersonCameraOffset : FVector::ZeroVector;
}

void AShipBase::SetSkirmishManager(
    ASkirmishManager* NewManager
)
{
    OwningSkirmishManager = NewManager;
}

void AShipBase::ExitAreaViaHyperspace()
{
	SetCombatEnabled(false);
	PlayHyperspaceExitEffect();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
}

void AShipBase::SetCombatEnabled(bool bEnabled)
{
	if (bCombatEnabled == bEnabled)
	{
		return;
	}

	bCombatEnabled = bEnabled;

	if (!bCombatEnabled)
	{
		ThrottleInput = 0.0f;
		PitchInput = 0.0f;
		YawInput = 0.0f;
		RollInput = 0.0f;

		StopFiringLasers();

		if (SteeringComponent)
		{
			SteeringComponent->StopSteering();
			SteeringComponent->Deactivate();
		}

		if (LaserWeaponComponent)
		{
			LaserWeaponComponent->Deactivate();
		}

		if (MissileWeaponComponent)
		{
			MissileWeaponComponent->Deactivate();
		}

		if (TargetingComponent)
		{
			TargetingComponent->Deactivate();
		}

		if (SensingComponent)
		{
			SensingComponent->Deactivate();
		}

		if (ShipMesh)
		{
			ShipMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			ShipMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		}

		return;
	}

	if (IsDead())
	{
		return;
	}

	if (LaserWeaponComponent)
	{
		LaserWeaponComponent->Activate();
	}

	if (MissileWeaponComponent)
	{
		MissileWeaponComponent->Activate();
	}

	if (TargetingComponent)
	{
		TargetingComponent->Activate();
	}

	if (SensingComponent)
	{
		SensingComponent->Activate();
	}

	if (SteeringComponent)
	{
		SteeringComponent->Activate();
	}

	if (ShipMesh)
	{
		ShipMesh->SetPhysicsLinearVelocity(
			GetActorForwardVector() * CurrentSpeed
		);
	}
}

bool AShipBase::IsCombatEnabled() const
{
	return bCombatEnabled;
}

void AShipBase::SetPreMatchFlight(bool bEnabled)
{
    bPreMatchFlight = bEnabled;

    ThrottleInput = 0.0f;
    PitchInput = 0.0f;
    YawInput = 0.0f;
    RollInput = 0.0f;

    StopFiringLasers();

    if (ShipMesh)
    {
        ShipMesh->SetPhysicsAngularVelocityInRadians(
            FVector::ZeroVector
        );

        ShipMesh->SetPhysicsLinearVelocity(
            GetActorForwardVector() * CurrentSpeed
        );
    }
}