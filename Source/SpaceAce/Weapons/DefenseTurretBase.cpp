#include "DefenseTurretBase.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "ShipBase.h"
#include "Net/UnrealNetwork.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ADefenseTurretBase::ADefenseTurretBase()
{
	PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval=.05f;
    bReplicates=true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(15.f);
    SetNetCullDistanceSquared(FMath::Square(90000.f));
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	FixedBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixedBaseMesh"));
	FixedBaseMesh->SetupAttachment(Root);
	YawBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("YawBaseMesh"));
	YawBaseMesh->SetupAttachment(FixedBaseMesh);
	PitchAssembly = CreateDefaultSubobject<USceneComponent>(TEXT("PitchAssembly"));
	PitchAssembly->SetupAttachment(YawBaseMesh);
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(PitchAssembly);
	PrimaryMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("PrimaryMuzzle"));
	PrimaryMuzzle->SetupAttachment(PitchAssembly);
	SecondaryMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("SecondaryMuzzle"));
	SecondaryMuzzle->SetupAttachment(PitchAssembly);
}

void ADefenseTurretBase::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
}

void ADefenseTurretBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDestroyed) return;
    if (!HasAuthority())
    {
        YawBaseMesh->SetRelativeRotation(FMath::RInterpTo(YawBaseMesh->GetRelativeRotation(),FRotator(0,ReplicatedYaw,0),DeltaSeconds,15));
        PitchAssembly->SetRelativeRotation(FMath::RInterpTo(PitchAssembly->GetRelativeRotation(),FRotator(ReplicatedPitch,0,0),DeltaSeconds,15));
        return;
    }

	FireTimer = FMath::Max(0.0f, FireTimer - DeltaSeconds);
	TargetScanTimer-=DeltaSeconds;
    if (TargetScanTimer<=0 || !CachedTarget.IsValid() || CachedTarget->IsDead())
    {
        CachedTarget=FindTarget(); TargetScanTimer=.25f;
    }
    AShipBase* Target = CachedTarget.Get();
	if (!Target) return;

	if (AimAtTarget(Target, DeltaSeconds) && FireTimer <= 0.0f && HasLineOfSightTo(Target))
	{
		FireAtTarget(Target);
		FireTimer = FireInterval;
	}
}

float ADefenseTurretBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !CanBeDamaged() || bDestroyed || !FMath::IsFinite(DamageAmount) || DamageAmount<=0) return 0.0f;
	const float Applied = FMath::Max(0.0f, DamageAmount);
	Health -= Applied;
	if (Health <= 0.0f)
	{
		bDestroyed = true;
        ForceNetUpdate();
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
	}
	return Applied;
}

AShipBase* ADefenseTurretBase::FindTarget() const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_ADefenseTurretBase_FindTarget);
	TArray<AActor*> Ships;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShipBase::StaticClass(), Ships);
	AShipBase* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(Range);

	for (AActor* Actor : Ships)
	{
		AShipBase* Ship = Cast<AShipBase>(Actor);
		if (!Ship || Ship->IsDead() || Ship->bOnCatapult || Ship->GetTeamID() == TeamID) continue;

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Ship->GetActorLocation());
		if (DistanceSquared > BestDistanceSquared || !CanAimAtLocation(Ship->GetActorLocation())) continue;
		if (bRequireLineOfSight && !HasLineOfSightTo(Ship)) continue;

		BestTarget = Ship;
		BestDistanceSquared = DistanceSquared;
	}
	return BestTarget;
}

bool ADefenseTurretBase::GetDesiredLocalAim(const FVector& WorldLocation,
	float& OutYaw, float& OutPitch) const
{
	if (!Root) return false;
	const FVector WorldDirection = WorldLocation - GetMuzzleLocation();
	if (WorldDirection.IsNearlyZero()) return false;

	const FVector LocalDirection = Root->GetComponentTransform()
		.InverseTransformVectorNoScale(WorldDirection).GetSafeNormal();
	const FRotator LocalAim = LocalDirection.Rotation();
	OutYaw = FMath::UnwindDegrees(LocalAim.Yaw);
	OutPitch = FMath::UnwindDegrees(LocalAim.Pitch);
	return true;
}

bool ADefenseTurretBase::CanAimAtLocation(const FVector& WorldLocation) const
{
	float DesiredYaw = 0.0f;
	float DesiredPitch = 0.0f;
	if (!GetDesiredLocalAim(WorldLocation, DesiredYaw, DesiredPitch)) return false;
	return DesiredYaw >= MinimumYaw && DesiredYaw <= MaximumYaw &&
		DesiredPitch >= MinimumPitch && DesiredPitch <= MaximumPitch;
}

bool ADefenseTurretBase::AimAtTarget(const AActor* Target, float DeltaSeconds)
{
	if (!Target || !YawBaseMesh || !PitchAssembly) return false;

	float DesiredYaw = 0.0f;
	float DesiredPitch = 0.0f;
	if (!GetDesiredLocalAim(Target->GetActorLocation(), DesiredYaw, DesiredPitch)) return false;
	if (DesiredYaw < MinimumYaw || DesiredYaw > MaximumYaw ||
		DesiredPitch < MinimumPitch || DesiredPitch > MaximumPitch) return false;

	const float CurrentYaw = FMath::UnwindDegrees(YawBaseMesh->GetRelativeRotation().Yaw);
	const float CurrentPitch = FMath::UnwindDegrees(PitchAssembly->GetRelativeRotation().Pitch);
	const float NewYaw = FMath::FInterpConstantTo(CurrentYaw, DesiredYaw,
		DeltaSeconds, YawSpeedDegreesPerSecond);
	const float NewPitch = FMath::FInterpConstantTo(CurrentPitch, DesiredPitch,
		DeltaSeconds, PitchSpeedDegreesPerSecond);

	YawBaseMesh->SetRelativeRotation(FRotator(0.0f, NewYaw, 0.0f));
	PitchAssembly->SetRelativeRotation(FRotator(NewPitch, 0.0f, 0.0f));

	ReplicatedYaw=NewYaw; ReplicatedPitch=NewPitch;
	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, DesiredYaw));
	const float PitchError = FMath::Abs(FMath::FindDeltaAngleDegrees(NewPitch, DesiredPitch));
	return YawError <= FireToleranceDegrees && PitchError <= FireToleranceDegrees;
}

bool ADefenseTurretBase::HasLineOfSightTo(const AActor* Target) const
{
	if (!bRequireLineOfSight || !Target || !GetWorld()) return true;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TurretLineOfSight), false, this);
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, GetMuzzleLocation(),
		Target->GetActorLocation(), ECC_Visibility, QueryParams);
	return !bHit || Hit.GetActor() == Target;
}

FVector ADefenseTurretBase::GetMuzzleLocation() const
{
	return PrimaryMuzzle ? PrimaryMuzzle->GetComponentLocation() : GetActorLocation();
}

FRotator ADefenseTurretBase::GetMuzzleRotation() const
{
	return PrimaryMuzzle ? PrimaryMuzzle->GetComponentRotation() : GetActorRotation();
}

void ADefenseTurretBase::OnRep_Destroyed()
{
    SetActorEnableCollision(!bDestroyed);
    SetActorHiddenInGame(bDestroyed);
    SetActorTickEnabled(!bDestroyed);
}
void ADefenseTurretBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADefenseTurretBase,TeamID);
    DOREPLIFETIME(ADefenseTurretBase,Health);
    DOREPLIFETIME(ADefenseTurretBase,bDestroyed);
    DOREPLIFETIME(ADefenseTurretBase,ReplicatedYaw);
    DOREPLIFETIME(ADefenseTurretBase,ReplicatedPitch);
}
