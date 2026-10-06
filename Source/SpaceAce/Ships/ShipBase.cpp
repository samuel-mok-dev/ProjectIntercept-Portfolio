#include "ShipBase.h"
#include "../Tests/FactionBalanceTelemetry.h"
// Fill out your copyright notice in the Description page of Project Settings.


#include "FighterPropulsionComponent.h"
#include "FighterLandingGearComponent.h"
#include "CountermeasureComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
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
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SkirmishManager.h"
#include "SceneTransitionSubsystem.h"

// Sets default values
AShipBase::AShipBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

    // Byte rotation quantization jumps by ~1.4 degrees, visible in a chase camera.
    FRepMovement& Movement = GetReplicatedMovement_Mutable();
    Movement.RotationQuantizationLevel = ERotatorQuantization::ShortComponents;
    Movement.LocationQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
    Movement.VelocityQuantizationLevel = EVectorQuantization::RoundTwoDecimals;

	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	SetRootComponent(ShipMesh);
    LandingGear=CreateDefaultSubobject<UFighterLandingGearComponent>(TEXT("LandingGear"));
    LandingGear->SetupAttachment(ShipMesh);
    Countermeasures=CreateDefaultSubobject<UCountermeasureComponent>(TEXT("Countermeasures"));
    Countermeasures->SetupAttachment(ShipMesh);
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
    EngineLEffectComponent->SetCanEverAffectNavigation(false);

	EngineREffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EngineEffectR"));
	EngineREffectComponent->SetupAttachment(ShipMesh);
    EngineREffectComponent->SetCanEverAffectNavigation(false);

    Propulsion = CreateDefaultSubobject<UFighterPropulsionComponent>(TEXT("LayeredPropulsion"));
    Propulsion->SetupAttachment(ShipMesh);

	EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(ShipMesh);
	EngineAudio->bAutoActivate = false;
}

// Called when the game starts or when spawned
void AShipBase::BeginPlay()
{
	Super::BeginPlay();

    // Run after Blueprint defaults are applied so existing fighters use the same
    // host-authoritative interpolation instead of legacy hard corrections.
    SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);
    SetNetUpdateFrequency(60.0f);
    SetMinNetUpdateFrequency(30.0f);
    ShipMesh->bReplicatePhysicsToAutonomousProxy = true;


	if (UWorld* World = GetWorld())
	{
		if (UCombatantSubsystem* CombatantSubsystem =
			World->GetSubsystem<UCombatantSubsystem>())
		{
			CombatantSubsystem->RegisterCombatant(this);
		}
	}

	ApplyShipData();

	if (HasAuthority() && ShipMesh)
	{
		FVector InitialVelocity = GetActorForwardVector() * CurrentSpeed;
		ShipMesh->SetPhysicsLinearVelocity(InitialVelocity);
	}

	EnginePitchVariation = FMath::FRandRange(0.97f, 1.03f);
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
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_AShipBase_Tick);
	Super::Tick(DeltaTime);

    if(!USceneTransitionSubsystem::IsSceneReady(this))
    {
        if(ShipMesh->IsSimulatingPhysics())
        {ShipMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);ShipMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);}
        UpdateEngineAudio();return;
    }

    if (bOnCatapult)
    {
        if (HasAuthority()) UpdateCatapult(DeltaTime);
        else if (!bPreMatchFlight)
        if (const auto* GS=GetWorld()->GetGameState())
        {
            // Autonomous proxies don't consume kinematic RepMovement. Evaluate the
            // same server-timed rail curve locally, with no client launch authority.
            const float Distance=FVector::Distance(CatapultStart,CatapultExit);
            const float Speed=FMath::Max(1.f,ShipData?ShipData->CurrentSpeed:3000.f);
            const float Time=bCatapultLaunching ? FMath::Max(0.,GS->GetServerWorldTimeSeconds()-CatapultStartedAt) : 0;
            ShipMesh->SetSimulatePhysics(false);
            SetActorLocation(CatapultStart+(CatapultExit-CatapultStart).GetSafeNormal()*CatapultDistance(Time,Distance,Speed),false);
            CurrentSpeed=bCatapultLaunching?CatapultSpeed(Time,Distance,Speed):0;
        }
        UpdateEngineAudio();
        return;
    }

	if (!bCombatEnabled)
	{
		return;
	}

	// The server drives physics; clients consume replicated movement.
	if (!HasAuthority())
	{
		CurrentSpeed = GetVelocity().Size();
		UpdateEngineAudio();
        // Target selection and lock progress are replicated from the host.
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
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_AShipBase_ApplyShipData);
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
		MissileWeaponComponent->ApplySpecialWeaponProfile(ShipData->SpecialWeapon);
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

    // Reapplying ship data must restore both visibility and activation for twin engines.
    EngineLEffectComponent->SetAsset(ShipData->PropulsionMaterial ? nullptr : ShipData->EngineEffect);
    EngineLEffectComponent->SetRelativeLocation(ShipData->EngineLEffectOffset);
    EngineLEffectComponent->SetVisibility(ShipData->EngineEffect != nullptr && !ShipData->PropulsionMaterial);
    if (ShipData->EngineEffect && !ShipData->PropulsionMaterial) EngineLEffectComponent->Activate(true);
    else EngineLEffectComponent->Deactivate();

    EngineREffectComponent->SetAsset(ShipData->PropulsionMaterial ? nullptr : ShipData->EngineEffect);
    EngineREffectComponent->SetRelativeLocation(ShipData->EngineREffectOffset);
    const bool bUseRightEngine = ShipData->HasTwoEngines && ShipData->EngineEffect && !ShipData->PropulsionMaterial;
    EngineREffectComponent->SetVisibility(bUseRightEngine);
    if (bUseRightEngine) EngineREffectComponent->Activate(true);
    else EngineREffectComponent->Deactivate();

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
	FlightInput.Throttle = (bCombatEnabled || bOnCatapult) && !bPreMatchFlight ? Value : 0.0f;
}

void AShipBase::SetPitchInput(float Value)
{
	FlightInput.Pitch = bCombatEnabled && !bPreMatchFlight ? Value : 0.0f;
}

void AShipBase::SetYawInput(float Value)
{
	FlightInput.Yaw = bCombatEnabled && !bPreMatchFlight ? Value : 0.0f;
}

void AShipBase::SetRollInput(float Value)
{
	FlightInput.Roll = bCombatEnabled && !bPreMatchFlight ? Value : 0.0f;
}

// Update the current speed based on throttle input
void AShipBase::UpdateCurrentSpeed(float DeltaTime)
{
	if (!ShipMesh)
	{
		return;
	}

	float OldSpeed = CurrentSpeed;
	CurrentSpeed += FlightInput.Throttle * Acceleration * DeltaTime;
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
	const FVector BankTorque = GetShipUpVector() * -FlightInput.Roll * BankTurnStrength;
	
	const FVector PitchTorque = ShipMesh->GetRightVector() * FlightInput.Pitch * PitchSpeed;
	const FVector YawTorque = ShipMesh->GetUpVector() * FlightInput.Yaw * YawSpeed + BankTorque;
	const FVector RollTorque = ShipMesh->GetForwardVector() * FlightInput.Roll * RollSpeed;
	
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
    if (!HasAuthority())
    {
        if (IsLocallyControlled()) ServerSetLaserFiring(true);
        return;
    }
	if (bCombatEnabled && !IsDead() && LaserWeaponComponent && !bPreMatchFlight)
	{
		LaserWeaponComponent->StartFiringLasers();
	}
}

// Stop continuous gun firing
void AShipBase::StopFiringLasers()
{
    if (!HasAuthority())
    {
        if (IsLocallyControlled()) ServerSetLaserFiring(false);
        return;
    }
	if (LaserWeaponComponent)
	{
		LaserWeaponComponent->StopFiringLasers();
	}
}

// Fire ze missiles
void AShipBase::FireSingleMissile()
{
    if (!HasAuthority())
    {
        if (IsLocallyControlled()) ServerFireMissile();
        return;
    }
	if (IsDead() || !bCombatEnabled || !MissileWeaponComponent || !TargetingComponent || bPreMatchFlight)
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
    if (!HasAuthority())
    {
        if (IsLocallyControlled()) ServerSwitchSecondaryWeapon();
        return;
    }
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
    if (!HasAuthority())
    {
        if (IsLocallyControlled()) ServerSwitchTarget();
        return;
    }
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

float AShipBase::GetLockOnProgress() const
{
    return TargetingComponent ? TargetingComponent->GetLockOnProgress() : 0.0f;
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
    if (!HasAuthority() || !bCombatEnabled || bPreMatchFlight || !HealthComponent || IsDead() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
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
#if WITH_DEV_AUTOMATION_TESTS
        FactionBalance::DefenseDeath(this, DamageCauser);
#endif
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
    ForceNetUpdate();

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
		TargetingComponent->ResetLockOn();
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

	if (HasAuthority() && PlayerController)
	{
		PlayerController->UnPossess();
	}

	if (DeathExplosionEffect && GetNetMode() != NM_DedicatedServer)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			DeathExplosionEffect,
			GetActorLocation(),
			GetActorRotation(), FVector::OneVector, true, true, ENCPoolMethod::AutoRelease
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
    if (Countermeasures) Countermeasures->ResetSortie();
    ForceNetUpdate();
    if (TargetingComponent) TargetingComponent->ResetLockOn();
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
        MissileWeaponComponent->ResetForSortie();
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

	if (EngineLEffectComponent && ShipData && !ShipData->PropulsionMaterial && !EngineLEffectComponent->IsActive())
	{
		EngineLEffectComponent->Activate();
	}

	if (EngineREffectComponent && ShipData && ShipData->HasTwoEngines &&
        ShipData->EngineEffect && !ShipData->PropulsionMaterial && !EngineREffectComponent->IsActive())
	{
		EngineREffectComponent->Activate();
	}

	if (HasAuthority() && ShipMesh)
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
	return FlightInput.Throttle;
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

bool AShipBase::IsHostileTo(const AShipBase* OtherShip) const
{
    if (!IsValid(OtherShip) || OtherShip == this) return false;
    // Explicit match teams take precedence; old mission assets still use tags.
    if (RuntimeTeamID != INDEX_NONE && OtherShip->RuntimeTeamID != INDEX_NONE)
        return RuntimeTeamID != OtherShip->RuntimeTeamID;
    return EnemyTeamTag.IsValid() && OtherShip->TeamTag.MatchesTagExact(EnemyTeamTag);
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

    EngineAudio->SetPitchMultiplier(Pitch * EnginePitchVariation);
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

void AShipBase::SetMissionStaged(bool bStaged)
{
    if(!HasAuthority())return;
    SetCombatEnabled(!bStaged);SetActorHiddenInGame(bStaged);SetActorEnableCollision(!bStaged);
    ShipMesh->SetSimulatePhysics(!bStaged);SetActorTickEnabled(!bStaged);
    if(GetController())GetController()->SetActorTickEnabled(!bStaged);
    if(bStaged)
    {
        MissionPausedComponents.Reset();
        TInlineComponentArray<UActorComponent*> Components(this);
        for(auto* Component:Components)
        {
            if(Component->IsComponentTickEnabled()){MissionPausedComponents.Add(Component);Component->SetComponentTickEnabled(false);}
            if(auto* Audio=Cast<UAudioComponent>(Component))Audio->SetPaused(true);
            if(auto* FX=Cast<UNiagaraComponent>(Component))FX->SetPaused(true);
        }
    }
    else
    {
        for(auto& Component:MissionPausedComponents)if(Component.IsValid())Component->SetComponentTickEnabled(true);
        MissionPausedComponents.Reset();
        TInlineComponentArray<UActorComponent*> Components(this);
        for(auto* Component:Components)
        {
            if(auto* Audio=Cast<UAudioComponent>(Component))Audio->SetPaused(false);
            if(auto* FX=Cast<UNiagaraComponent>(Component))FX->SetPaused(false);
        }
        ShipMesh->SetPhysicsLinearVelocity(GetActorForwardVector()*CurrentSpeed);
    }
}

void AShipBase::SetCombatEnabled(bool bEnabled)
{
    if (bEnabled && bOnCatapult) return;
	if (bCombatEnabled == bEnabled)
	{
		return;
	}

	bCombatEnabled = bEnabled;

	if (!bCombatEnabled)
	{
		FlightInput.Throttle = 0.0f;
		FlightInput.Pitch = 0.0f;
		FlightInput.Yaw = 0.0f;
		FlightInput.Roll = 0.0f;

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
			TargetingComponent->ResetLockOn();
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
    if (TargetingComponent) TargetingComponent->ResetLockOn();

    FlightInput.Throttle = 0.0f;
    FlightInput.Pitch = 0.0f;
    FlightInput.Yaw = 0.0f;
    FlightInput.Roll = 0.0f;

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

void AShipBase::SetFlightInput(const FFlightInput& NewFlightInput)
{
    const auto SanitizeAxis = [](float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f;
    };
    SetThrottleInput(SanitizeAxis(NewFlightInput.Throttle));
    SetPitchInput(SanitizeAxis(NewFlightInput.Pitch));
    SetYawInput(SanitizeAxis(NewFlightInput.Yaw));
    SetRollInput(SanitizeAxis(NewFlightInput.Roll));
}
void AShipBase::ServerSetLaserFiring_Implementation(bool bFiring)
{
    if (bFiring) StartFiringLasers(); else StopFiringLasers();
}
void AShipBase::ServerFireMissile_Implementation() { FireSingleMissile(); }
void AShipBase::ServerSwitchTarget_Implementation()
{
    if (!IsDead() && IsCombatEnabled() && !IsInPreMatchFlight()) SwitchTarget();
}
void AShipBase::ServerSwitchSecondaryWeapon_Implementation()
{
    if (!IsDead() && IsCombatEnabled() && !IsInPreMatchFlight()) SwitchSecondaryWeaponMode();
}
void AShipBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AShipBase, ShipData);
    DOREPLIFETIME(AShipBase, RuntimeTeamID);
    DOREPLIFETIME(AShipBase, bCombatEnabled);
    DOREPLIFETIME(AShipBase, bPreMatchFlight);
    DOREPLIFETIME(AShipBase, bDeathHandled);
    DOREPLIFETIME(AShipBase, bOnCatapult);
    DOREPLIFETIME(AShipBase, bCatapultLaunching);
    DOREPLIFETIME(AShipBase, CatapultStart);
    DOREPLIFETIME(AShipBase, CatapultExit);
    DOREPLIFETIME(AShipBase, CatapultStartedAt);
    DOREPLIFETIME_CONDITION(AShipBase, CatapultWait, COND_OwnerOnly);
}
void AShipBase::OnRep_DeathHandled()
{
    if (bDeathHandled)
    {
        bDeathHandled = false;
        HandleDeath();
    }
    else
    {
        const bool bWasCombatEnabled = bCombatEnabled;
        ResetShip();
        bCombatEnabled = bWasCombatEnabled;
    }
}
void AShipBase::OnRep_ShipData() { ApplyShipData(); }

ASkirmishManager* AShipBase::GetSkirmishManager() const { return OwningSkirmishManager.Get(); }

float AShipBase::CatapultDistance(float Time, float Distance, float ExitSpeed)
{
    if (Distance <= 0 || ExitSpeed <= 0 || Time <= 0) return 0;
    // A short, hard stroke, then guided travel at cruise speed to the exit.
    const float StrokeTime = FMath::Min(1.6f, 2.f * Distance / ExitSpeed);
    const float AcceleratingTime = FMath::Min(Time, StrokeTime);
    return FMath::Min(Distance, .5f * ExitSpeed / StrokeTime * FMath::Square(AcceleratingTime)
        + ExitSpeed * FMath::Max(0.f, Time-StrokeTime));
}

float AShipBase::CatapultSpeed(float Time, float Distance, float ExitSpeed)
{
    if (Distance <= 0 || ExitSpeed <= 0) return 0;
    return ExitSpeed * FMath::Clamp(Time / FMath::Min(1.6f, 2.f * Distance / ExitSpeed), 0.f, 1.f);
}

void AShipBase::OnRep_CatapultStatus()
{
    // The movement packet and gameplay flag may arrive on different frames.
    // Always restore simulation when the authoritative launch finishes.
    ShipMesh->SetSimulatePhysics(!bOnCatapult && !IsDead());
}

void AShipBase::BeginCatapult(const FTransform& Start, const FVector& Exit)
{
    if (!HasAuthority() || IsDead()) return;
    SetCombatEnabled(false);
    bOnCatapult = true;
    bCatapultLaunching = false;
    CatapultHold = CatapultElapsed = CatapultWait = 0;
    CatapultStart = Start.GetLocation();
    CatapultExit = Exit;
    CurrentSpeed = 0;
    ShipMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    ShipMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    ShipMesh->SetSimulatePhysics(false);
    SetActorLocationAndRotation(Start.GetLocation(), Start.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    ForceNetUpdate();
}

void AShipBase::UpdateCatapult(float DeltaTime)
{
    if (!HasAuthority() || !bOnCatapult || IsDead() || bPreMatchFlight) return;
    if(LandingGear && ShipData && ShipData->LandingGearBays.Num()==3 && LandingGear->GetDeployment()<.999f)return;
    if (!bCatapultLaunching)
    {
        CatapultWait+=FMath::Max(0.f,DeltaTime);
        const bool bAutomatic = GetController() && !GetController()->IsPlayerController();
        if(bAutomatic && LaunchLeader.IsValid() && !LaunchLeader->IsDead() && LaunchLeader->bOnCatapult && !LaunchLeader->bCatapultLaunching)return;
        CatapultHold = (bAutomatic || FlightInput.Throttle > .5f) ? CatapultHold + DeltaTime : 0.f;
        if (CatapultHold < .35f) return;
        bCatapultLaunching = true;
        // Commit the launch, raise the deflector, then release the catapult.
        CatapultElapsed=-.65f;
        CatapultStartedAt=GetWorld()->GetTimeSeconds()+.65f;
        ForceNetUpdate();
        return;
    }
    const float Distance = FVector::Distance(CatapultStart, CatapultExit);
    const float Speed = FMath::Max(1.f, ShipData ? ShipData->CurrentSpeed : 3000.f);
    const float Duration = Distance / Speed + .5f * FMath::Min(1.6f,2.f*Distance/Speed);
    CatapultElapsed += DeltaTime;
    const FVector Direction = (CatapultExit-CatapultStart).GetSafeNormal();
    CurrentSpeed = CatapultSpeed(CatapultElapsed,Distance,Speed);
    SetActorLocation(CatapultStart + Direction * CatapultDistance(CatapultElapsed,Distance,Speed),false);
    if (CatapultElapsed >= Duration)
    {
        bOnCatapult = bCatapultLaunching = false;
        CurrentSpeed = Speed;
        ShipMesh->SetSimulatePhysics(true);
        SetCombatEnabled(true);
        ShipMesh->SetPhysicsLinearVelocity(Direction*Speed);
        FlightInput = FFlightInput{};
        RefreshTargets();
        ForceNetUpdate();
    }
}
