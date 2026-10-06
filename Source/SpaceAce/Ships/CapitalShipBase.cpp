#include "CapitalShipBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "CapitalShipDataBase.h"
#include "DefenseTurretBase.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ACapitalShipBase::ACapitalShipBase()
{
	PrimaryActorTick.bCanEverTick = true;
	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	SetRootComponent(ShipMesh);
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(ShipMesh);
	EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(ShipMesh);
	EngineAudio->bAutoActivate = false;
}

void ACapitalShipBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyCapitalShipData();
}

void ACapitalShipBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateMovement(DeltaTime);
}

void ACapitalShipBase::ApplyCapitalShipData()
{
	if (!CapitalShipData) return;
	ClearSpawnedAttachments();
	ShipMesh->SetStaticMesh(CapitalShipData->CapitalShipMesh);
	ForwardSpeed = CapitalShipData->MovementSpeed;
	Acceleration = CapitalShipData->Acceleration;
	RotationSpeed = CapitalShipData->RotationSpeed;
	CurrentHealth = CapitalShipData->MaxHealth;
	bDestroyed = false;
	EngineAudio->SetSound(CapitalShipData->EngineSound);
	if (CapitalShipData->EngineSound) EngineAudio->Play();
	SpawnEngineEffects();
	SpawnWeapons();
	SpawnObjectives();

	JumpSpeed = CapitalShipData->JumpSpeed;
	JumpEntryDistance = CapitalShipData->JumpEntryDistance;
	JumpExitDistance = CapitalShipData->JumpExitDistance;
	JumpOutAlignmentTolerance =
		CapitalShipData->JumpOutAlignmentTolerance;
}

void ACapitalShipBase::ClearSpawnedAttachments()
{
	for (ADefenseTurretBase* Weapon : SpawnedWeapons)
	{
		if (IsValid(Weapon)) Weapon->Destroy();
	}
	for (AActor* Objective : ObjectiveActors)
	{
		if (IsValid(Objective) && !Objective->IsA<ADefenseTurretBase>())
		{
			Objective->Destroy();
		}
	}
	for (UNiagaraComponent* Effect : EngineEffectComponents)
	{
		if (IsValid(Effect)) Effect->DestroyComponent();
	}
	SpawnedWeapons.Empty();
	ObjectiveActors.Empty();
	EngineEffectComponents.Empty();
}

void ACapitalShipBase::SpawnWeapons()
{
	if (!HasAuthority() || !CapitalShipData || !GetWorld() || !ShouldSpawnWeapons()) return;
#if WITH_DEV_AUTOMATION_TESTS
    if (FParse::Param(FCommandLine::Get(),TEXT("SpaceAceBalance")) &&
        FParse::Param(FCommandLine::Get(),TEXT("SpaceAceBalanceNoDefenses"))) return;
#endif
	for (const FCapitalShipWeaponDefinition& Definition : CapitalShipData->Weapons)
	{
		if (!Definition.WeaponClass) continue;
		const FTransform ParentTransform = Definition.SocketName.IsNone()
			? ShipMesh->GetComponentTransform()
			: ShipMesh->GetSocketTransform(Definition.SocketName, RTS_World);
		const FTransform SpawnTransform = Definition.RelativeTransform * ParentTransform;
		ADefenseTurretBase* Weapon = GetWorld()->SpawnActorDeferred<ADefenseTurretBase>(
			Definition.WeaponClass, SpawnTransform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Weapon) continue;
		Weapon->SetTeamID(TeamID);
		UGameplayStatics::FinishSpawningActor(Weapon, SpawnTransform);
		Weapon->AttachToComponent(ShipMesh, FAttachmentTransformRules::KeepWorldTransform,
			Definition.SocketName);
		SpawnedWeapons.Add(Weapon);
		if (Definition.bCountsAsMissionObjective) ObjectiveActors.Add(Weapon);
	}
}

void ACapitalShipBase::SpawnObjectives()
{
	if (!HasAuthority() || !CapitalShipData || !GetWorld()) return;
	for (const FCapitalShipObjectiveDefinition& Definition : CapitalShipData->Objectives)
	{
		if (!Definition.ObjectiveClass) continue;
		const FTransform ParentTransform = Definition.SocketName.IsNone()
			? ShipMesh->GetComponentTransform()
			: ShipMesh->GetSocketTransform(Definition.SocketName, RTS_World);
		const FTransform SpawnTransform = Definition.RelativeTransform * ParentTransform;
		AActor* Objective = GetWorld()->SpawnActorDeferred<AActor>(Definition.ObjectiveClass,
			SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Objective) continue;
		UGameplayStatics::FinishSpawningActor(Objective, SpawnTransform);
		Objective->AttachToComponent(ShipMesh, FAttachmentTransformRules::KeepWorldTransform,
			Definition.SocketName);
		ObjectiveActors.Add(Objective);
	}
}

int32 ACapitalShipBase::GetExpectedMissionObjectiveCount() const
{
    if (!CapitalShipData) return 0;
    int32 Count = 0;
    for (const auto& Weapon : CapitalShipData->Weapons)
        if (Weapon.bCountsAsMissionObjective) ++Count;
    Count += CapitalShipData->Objectives.Num();
    return Count;
}

void ACapitalShipBase::SpawnEngineEffects()
{
    if (!CapitalShipData || !CapitalShipData->EngineEffect)
    {
        return;
    }

    for (const FTransform& EngineTransform :
        CapitalShipData->EngineTransforms)
    {
        UNiagaraComponent* Effect =
            UNiagaraFunctionLibrary::SpawnSystemAttached(
                CapitalShipData->EngineEffect,
                ShipMesh,
                NAME_None,
                EngineTransform.GetLocation(),
                EngineTransform.Rotator(),
                EAttachLocation::KeepRelativeOffset,
                true,
                true
            );

        if (Effect)
        {
            Effect->SetRelativeScale3D(
                EngineTransform.GetScale3D()
            );

            EngineEffectComponents.Add(Effect);
        }
    }
}

void ACapitalShipBase::SetTeamID(int32 NewTeamID)
{
	TeamID = NewTeamID;
	for (ADefenseTurretBase* Weapon : SpawnedWeapons)
	{
		if (IsValid(Weapon)) Weapon->SetTeamID(TeamID);
	}
}

void ACapitalShipBase::MoveToLocation(const FVector& NewTargetLocation)
{
	TargetLocation = NewTargetLocation;
	MovementState = ECapitalShipMovementState::MovingToLocation;
}

void ACapitalShipBase::HoldPosition()
{
	MovementState = ECapitalShipMovementState::Holding;
}

void ACapitalShipBase::JumpIn(
    const FVector& ArrivalLocation,
    const FVector& ArrivalDirection
)
{
    JumpDirection = ArrivalDirection.GetSafeNormal();

    if (JumpDirection.IsNearlyZero())
    {
        JumpDirection = GetActorForwardVector();
    }

    JumpArrivalLocation = ArrivalLocation;

    const FVector EntryLocation =
        JumpArrivalLocation -
        JumpDirection * JumpEntryDistance;

    SetActorLocation(EntryLocation);
    SetActorRotation(JumpDirection.Rotation());

    SetActorHiddenInGame(false);

    // Avoid colliding with another ship while moving at hyperspace speed.
    SetActorEnableCollision(false);

    CurrentSpeed = 0.0f;
    MovementState = ECapitalShipMovementState::JumpingIn;

    OnJumpInStarted();
}

void ACapitalShipBase::JumpOut(
    const FVector& ExitDirection
)
{
    JumpDirection = ExitDirection.GetSafeNormal();

    if (JumpDirection.IsNearlyZero())
    {
        JumpDirection = GetActorForwardVector();
    }

    TargetLocation =
        GetActorLocation() +
        JumpDirection * 1000000.0f;

    CurrentSpeed = 0.0f;

    MovementState =
        ECapitalShipMovementState::AligningForJumpOut;
}

void ACapitalShipBase::UpdateMovement(float DeltaTime)
{
    switch (MovementState)
    {
    case ECapitalShipMovementState::Holding:
    {
        CurrentSpeed = FMath::FInterpConstantTo(
            CurrentSpeed,
            0.0f,
            DeltaTime,
            Acceleration
        );

        break;
    }

    case ECapitalShipMovementState::MovingToLocation:
    {
        const float Distance = FVector::Distance(
            GetActorLocation(),
            TargetLocation
        );

        if (Distance <= ArrivalAcceptanceRadius)
        {
            HoldPosition();
            CompleteManeuver();
            break;
        }

        SteerTowardsLocation(TargetLocation, DeltaTime);

        const float BrakingSpeed = FMath::Sqrt(
            FMath::Max(
                0.0f,
                2.0f * Acceleration * Distance
            )
        );

        const float DesiredSpeed = FMath::Min(
            ForwardSpeed,
            BrakingSpeed
        );

        CurrentSpeed = FMath::FInterpConstantTo(
            CurrentSpeed,
            DesiredSpeed,
            DeltaTime,
            Acceleration
        );

        AddActorWorldOffset(
            GetActorForwardVector() *
            CurrentSpeed *
            DeltaTime,
            true
        );

        break;
    }

    case ECapitalShipMovementState::JumpingIn:
    {
        const FVector ToArrival =
            JumpArrivalLocation - GetActorLocation();

        const float RemainingDistance =
            ToArrival.Size();

        const float MovementThisFrame =
            JumpSpeed * DeltaTime;

        if (RemainingDistance <= MovementThisFrame)
        {
            SetActorLocation(JumpArrivalLocation);

            CurrentSpeed = 0.0f;
            MovementState =
                ECapitalShipMovementState::Holding;

            SetActorEnableCollision(true);

            OnJumpInFinished();
            CompleteManeuver();

            break;
        }

        AddActorWorldOffset(
            JumpDirection * MovementThisFrame,
            false
        );

        break;
    }

    case ECapitalShipMovementState::AligningForJumpOut:
    {
        SteerTowardsLocation(TargetLocation, DeltaTime);

        const float Alignment = FVector::DotProduct(
            GetActorForwardVector().GetSafeNormal(),
            JumpDirection
        );

        const float AlignmentDegrees =
            FMath::RadiansToDegrees(
                FMath::Acos(
                    FMath::Clamp(
                        Alignment,
                        -1.0f,
                        1.0f
                    )
                )
            );

        if (AlignmentDegrees <=
            JumpOutAlignmentTolerance)
        {
            JumpStartLocation = GetActorLocation();

            SetActorEnableCollision(false);

            MovementState =
                ECapitalShipMovementState::JumpingOut;

            OnJumpOutStarted();
        }

        break;
    }

    case ECapitalShipMovementState::JumpingOut:
    {
        AddActorWorldOffset(
            JumpDirection *
            JumpSpeed *
            DeltaTime,
            false
        );

        const float DistanceTravelled =
            FVector::Distance(
                JumpStartLocation,
                GetActorLocation()
            );

        if (DistanceTravelled >= JumpExitDistance)
        {
            CurrentSpeed = 0.0f;

            MovementState =
                ECapitalShipMovementState::Holding;

            SetActorHiddenInGame(true);

            OnJumpOutFinished();
            CompleteManeuver();
        }

        break;
    }
    }
}

void ACapitalShipBase::SteerTowardsLocation(const FVector& Location, float DeltaTime)
{
	const FVector Direction = Location - GetActorLocation();
	if (Direction.IsNearlyZero()) return;
	const FRotator DesiredRotation = Direction.Rotation();
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), DesiredRotation,
		DeltaTime, RotationSpeed));
}

void ACapitalShipBase::CompleteManeuver()
{
	OnManeuverCompleted.Broadcast();
}

float ACapitalShipBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !CanBeDamaged() || bDestroyed) return 0.0f;
	const float AppliedDamage = FMath::Max(0.0f, DamageAmount);
	CurrentHealth -= AppliedDamage;
	if (CurrentHealth <= 0.0f)
	{
		bDestroyed = true;
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
	}
	return AppliedDamage;
}

bool ACapitalShipBase::IsMissionObjectiveComplete_Implementation() const
{
	if (bDestroyed) return true;
	if (ObjectiveActors.IsEmpty()) return false;
	for (AActor* Objective : ObjectiveActors)
	{
		if (!IsValid(Objective)) continue;
		if (!Objective->GetClass()->ImplementsInterface(UMissionObjectiveInterface::StaticClass()))
		{
			return false;
		}
		if (!IMissionObjectiveInterface::Execute_IsMissionObjectiveComplete(Objective))
		{
			return false;
		}
	}
	return true;
}
