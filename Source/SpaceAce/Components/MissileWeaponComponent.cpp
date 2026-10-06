#include "MissileWeaponComponent.h"
#include "Net/UnrealNetwork.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "MissileProjectile.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "DefenseTurretBase.h"
#include "CapitalShipBase.h"
#include "GameFramework/GameStateBase.h"
#include "TargetingComponent.h"

UMissileWeaponComponent::UMissileWeaponComponent()
{
    SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UMissileWeaponComponent::Configure(
	const TArray<USceneComponent*>& NewHardpoints
)
{
	MissileHardpoints = NewHardpoints;
    ResetForSortie();
}

void UMissileWeaponComponent::ResetForSortie()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    for (auto Projectile : StandardMissilePool) if (IsValid(Projectile) && !Projectile->IsAvailable()) Projectile->DeactivateProjectile();
    for (auto Projectile : SpecialWeaponPool) if (IsValid(Projectile) && !Projectile->IsAvailable()) Projectile->DeactivateProjectile();
    if (GetWorld()) for (auto& Handle : MissileReloadTimerHandles) GetWorld()->GetTimerManager().ClearTimer(Handle);
	bHardpointLoaded.Init(true, MissileHardpoints.Num());
	MissileReloadTimerHandles.SetNum(MissileHardpoints.Num());
	RemainingSpecialAmmo = SpecialWeaponMaximumAmmo;
	NextSpecialHardpoint = 0;
    LastSpecialFireTime=-1000.0f;SpecialReadyAt=0.0f;ActiveProjectileCount=0;
    WeaponMode=ESecondaryWeaponMode::StandardMissile;
}

void UMissileWeaponComponent::InitializeMissilePool()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_UMissileWeaponComponent_InitializeMissilePool);
    for (auto Projectile : StandardMissilePool) if (IsValid(Projectile)) Projectile->Destroy();
    for (auto Projectile : SpecialWeaponPool) if (IsValid(Projectile)) Projectile->Destroy();
	StandardMissilePool.Empty();
	SpecialWeaponPool.Empty();
	SpecialWeaponProjectiles.Empty();

	AMissileProjectile* Projectile = nullptr;
	while (AvailableStandardMissiles.Dequeue(Projectile)) {}
	while (AvailableSpecialWeapons.Dequeue(Projectile)) {}

	BuildPool(MissileProjectileClass, MissilePoolSize, false);

	if (SpecialProjectileClass)
	{
		BuildPool(SpecialProjectileClass, SpecialPoolSize, true);
	}
}

void UMissileWeaponComponent::BuildPool(
	TSubclassOf<AMissileProjectile> ProjectileClass,
	int32 PoolSize,
	bool bSpecialPool
)
{
	if (!ProjectileClass || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = GetOwner();
	Parameters.Instigator = Cast<APawn>(GetOwner());
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		AMissileProjectile* Projectile =
			GetWorld()->SpawnActor<AMissileProjectile>(
				ProjectileClass,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				Parameters
			);

		if (!Projectile)
		{
			continue;
		}

        Projectile->SetReplicates(true);
        Projectile->SetReplicateMovement(true);
		Projectile->OnMissileDeactivated.BindUObject(
			this,
			&UMissileWeaponComponent::ReturnMissileToPool
		);
		Projectile->DeactivateProjectile();

		if (bSpecialPool)
		{
			SpecialWeaponPool.Add(Projectile);
			SpecialWeaponProjectiles.Add(Projectile);
			AvailableSpecialWeapons.Enqueue(Projectile);
		}
		else
		{
			StandardMissilePool.Add(Projectile);
			AvailableStandardMissiles.Enqueue(Projectile);
		}
	}
}

AMissileProjectile* UMissileWeaponComponent::GetAvailableProjectile(
	bool bSpecialPool
)
{
	TQueue<AMissileProjectile*>& Queue = bSpecialPool
		? AvailableSpecialWeapons
		: AvailableStandardMissiles;

	AMissileProjectile* Projectile = nullptr;
	while (Queue.Dequeue(Projectile))
	{
		if (IsValid(Projectile) && Projectile->IsAvailable())
		{
			return Projectile;
		}
	}

	return nullptr;
}

void UMissileWeaponComponent::FireMissile(AActor* Target, bool bHasLock)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_UMissileWeaponComponent_FireMissile);
	if (WeaponMode == ESecondaryWeaponMode::SpecialWeapon)
	{
		FireSpecialWeapon(Target, bHasLock);
	}
	else
	{
		FireStandardMissile(Target, bHasLock);
	}
}

void UMissileWeaponComponent::FireStandardMissile(
	AActor* Target,
	bool bHasLock
)
{
	for (int32 Index = 0; Index < MissileHardpoints.Num(); ++Index)
	{
		if (!bHardpointLoaded.IsValidIndex(Index) ||
			!bHardpointLoaded[Index] ||
			!IsValid(MissileHardpoints[Index]))
		{
			continue;
		}

		AMissileProjectile* Missile = GetAvailableProjectile(false);
		if (!Missile)
		{
			return;
		}

		Missile->InitializeProjectile(
			MissileSpeed,
			MissileDamage,
			MissileLifetime
		);
		Missile->ConfigureFlight(
			bStandardMissileIsHoming,
			bStandardMissileInheritsShipVelocity ? GetOwner()->GetVelocity() : FVector::ZeroVector,
			StandardMissileGravityScale
		);
		Missile->ActivateProjectile(
			MissileHardpoints[Index]->GetComponentLocation(),
			MissileHardpoints[Index]->GetComponentRotation(),
			GetOwner(),
			MissileDamage
		);

		if (bStandardMissileIsHoming && bHasLock && Target)
		{
			Missile->SetTarget(Target);
		}

		bHardpointLoaded[Index] = false;
		ReloadMissile(Index);
		++ActiveProjectileCount;
		return;
	}
}

void UMissileWeaponComponent::FireSpecialWeapon(
	AActor* Target,
	bool bHasLock
)
{
	if (!GetWorld() || RemainingSpecialAmmo <= 0 || MissileHardpoints.IsEmpty())
	{
		return;
	}
    if (SpecialProfile.Type != ESpecialWeaponType::None && bSpecialWeaponIsHoming &&
        (!bHasLock || !IsTargetCompatible(Target))) return;

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastSpecialFireTime < SpecialWeaponCooldown)
	{
		return;
	}

	AMissileProjectile* Projectile = GetAvailableProjectile(true);
	if (!Projectile)
	{
		return;
	}

	const int32 HardpointIndex =
		NextSpecialHardpoint % MissileHardpoints.Num();
	USceneComponent* Hardpoint = MissileHardpoints[HardpointIndex];
	if (!IsValid(Hardpoint))
	{
		AvailableSpecialWeapons.Enqueue(Projectile);
		return;
	}

	Projectile->InitializeProjectile(
		SpecialWeaponSpeed,
		SpecialWeaponDamage,
		SpecialWeaponLifetime
	);
	Projectile->ConfigureFlight(
		bSpecialWeaponIsHoming,
		bSpecialWeaponInheritsShipVelocity ? GetOwner()->GetVelocity() : FVector::ZeroVector,
		SpecialWeaponGravityScale
	);
    Projectile->ConfigurePayload(SpecialProfile.TurnRate,SpecialProfile.TrackingAlignment,
        SpecialProfile.BlastRadius,SpecialProfile.AntiShipDamageMultiplier);
    const bool bBomb=SpecialProfile.Type==ESpecialWeaponType::UnguidedBomb;
	Projectile->ActivateProjectile(
		Hardpoint->GetComponentLocation(),
		bBomb ? (-GetOwner()->GetActorUpVector()).Rotation() : Hardpoint->GetComponentRotation(),
		GetOwner(),
		SpecialWeaponDamage
	);

	if (bSpecialWeaponIsHoming && bHasLock && Target)
	{
		Projectile->SetTarget(Target);
	}

	--RemainingSpecialAmmo;
	++NextSpecialHardpoint;
	++ActiveProjectileCount;
	LastSpecialFireTime = CurrentTime;
    SpecialReadyAt=CurrentTime+SpecialWeaponCooldown;
}

void UMissileWeaponComponent::SwitchWeaponMode()
{
	SetWeaponMode(
		WeaponMode == ESecondaryWeaponMode::StandardMissile
			? ESecondaryWeaponMode::SpecialWeapon
			: ESecondaryWeaponMode::StandardMissile
	);
}

void UMissileWeaponComponent::SetWeaponMode(ESecondaryWeaponMode NewMode)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
	if (NewMode == ESecondaryWeaponMode::SpecialWeapon && !SpecialProjectileClass)
	{
		WeaponMode = ESecondaryWeaponMode::StandardMissile;
		return;
	}

    if (WeaponMode != NewMode)
    {
        WeaponMode=NewMode;
        if (GetOwner()) if (auto* Targeting=GetOwner()->FindComponentByClass<UTargetingComponent>()) Targeting->ResetLockOn();
    }
}

void UMissileWeaponComponent::ReloadMissile(int32 HardpointIndex)
{
	if (!MissileReloadTimerHandles.IsValidIndex(HardpointIndex) ||
		!bHardpointLoaded.IsValidIndex(HardpointIndex) ||
		!GetWorld())
	{
		return;
	}

	FTimerDelegate Delegate;
	Delegate.BindUObject(
		this,
		&UMissileWeaponComponent::FinishReload,
		HardpointIndex
	);

	GetWorld()->GetTimerManager().SetTimer(
		MissileReloadTimerHandles[HardpointIndex],
		Delegate,
		MissileReloadTime,
		false
	);
}

void UMissileWeaponComponent::FinishReload(int32 HardpointIndex)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
	if (bHardpointLoaded.IsValidIndex(HardpointIndex))
	{
		bHardpointLoaded[HardpointIndex] = true;
	}
}

void UMissileWeaponComponent::ReturnMissileToPool(
	AMissileProjectile* Missile
)
{
	if (!IsValid(Missile))
	{
		return;
	}

	if (SpecialWeaponProjectiles.Contains(Missile))
	{
		AvailableSpecialWeapons.Enqueue(Missile);
	}
	else
	{
		AvailableStandardMissiles.Enqueue(Missile);
	}

	ActiveProjectileCount = FMath::Max(0, ActiveProjectileCount - 1);
}

bool UMissileWeaponComponent::HasLoadedMissile() const
{
	if (WeaponMode == ESecondaryWeaponMode::SpecialWeapon)
	{
		return SpecialProjectileClass && RemainingSpecialAmmo > 0 && GetCooldownRemaining() <= 0;
	}

	return bHardpointLoaded.Contains(true);
}

bool UMissileWeaponComponent::HasMissileInTheAir() const
{
	return ActiveProjectileCount > 0;
}

float UMissileWeaponComponent::GetEffectiveRange() const
{
    return WeaponMode==ESecondaryWeaponMode::SpecialWeapon && SpecialProfile.Type!=ESpecialWeaponType::None
        ? SpecialProfile.Range : EffectiveMissileRange;
}

int32 UMissileWeaponComponent::GetSpecialWeaponAmmo() const
{
	return RemainingSpecialAmmo;
}

ESecondaryWeaponMode UMissileWeaponComponent::GetWeaponMode() const
{
	return WeaponMode;
}

bool UMissileWeaponComponent::IsCurrentWeaponHoming() const
{
	return WeaponMode == ESecondaryWeaponMode::StandardMissile
		? bStandardMissileIsHoming
		: bSpecialWeaponIsHoming;
}

void UMissileWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UMissileWeaponComponent, WeaponMode, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UMissileWeaponComponent, bHardpointLoaded, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UMissileWeaponComponent, ActiveProjectileCount, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UMissileWeaponComponent, RemainingSpecialAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UMissileWeaponComponent, SpecialReadyAt, COND_OwnerOnly);
}

void UMissileWeaponComponent::ApplySpecialWeaponProfile(const FSpecialWeaponProfile& Profile)
{
    SpecialProfile=Profile;
    SpecialProjectileClass=Profile.ProjectileClass;
    SpecialWeaponMaximumAmmo=Profile.Type==ESpecialWeaponType::None ? 0 : Profile.Ammo;
    SpecialPoolSize=FMath::Min(6,SpecialWeaponMaximumAmmo);
    SpecialWeaponCooldown=Profile.Cooldown;SpecialWeaponDamage=Profile.Damage;
    SpecialWeaponSpeed=Profile.Speed;SpecialWeaponLifetime=Profile.Lifetime;
    bSpecialWeaponIsHoming=Profile.Type!=ESpecialWeaponType::UnguidedBomb && Profile.Type!=ESpecialWeaponType::None;
    bSpecialWeaponInheritsShipVelocity=Profile.Type==ESpecialWeaponType::UnguidedBomb;
    SpecialWeaponGravityScale=0.0f; // Space: bombs retain momentum and eject beneath the ship.
}

FString UMissileWeaponComponent::GetCurrentWeaponName() const
{
    return WeaponMode==ESecondaryWeaponMode::StandardMissile ? TEXT("MISSILE") :
        (SpecialProfile.DisplayName.IsEmpty() ? TEXT("SPECIAL") : SpecialProfile.DisplayName);
}

float UMissileWeaponComponent::GetRequiredLockTime() const
{
    return WeaponMode==ESecondaryWeaponMode::SpecialWeapon && SpecialProfile.Type!=ESpecialWeaponType::None ? SpecialProfile.LockTime : 1.0f;
}

float UMissileWeaponComponent::GetCooldownRemaining() const
{
    if (!GetWorld()) return 0;
    const auto* GS=GetWorld()->GetGameState();
    const float Time=GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    return FMath::Max(0.0f,SpecialReadyAt-Time);
}

bool UMissileWeaponComponent::IsTargetCompatible(const AActor* Target) const
{
    if (!IsValid(Target) || Target->IsHidden()) return false;
    return WeaponMode!=ESecondaryWeaponMode::SpecialWeapon || SpecialProfile.Type!=ESpecialWeaponType::AntiShip ||
        Target->IsA<ADefenseTurretBase>() || Target->IsA<ACapitalShipBase>();
}

bool UMissileWeaponComponent::PredictBombImpact(FVector& OutLocation,const AActor* RequiredActor) const
{
    if (!GetWorld() || !GetOwner() || SpecialProfile.Type!=ESpecialWeaponType::UnguidedBomb || MissileHardpoints.IsEmpty()) return false;
    const auto* Hardpoint=MissileHardpoints[NextSpecialHardpoint%MissileHardpoints.Num()];
    if (!Hardpoint) return false;
    const FVector Start=Hardpoint->GetComponentLocation();
    const FVector End=Start+(GetOwner()->GetVelocity()-GetOwner()->GetActorUpVector()*SpecialWeaponSpeed)*SpecialWeaponLifetime;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BombImpactPreview),false,GetOwner());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params)) return false;
    if(RequiredActor && Hit.GetActor()!=RequiredActor)return false;
    OutLocation=Hit.ImpactPoint;return true;
}

void UMissileWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    if (GetOwner() && GetOwner()->HasAuthority())
    {
    for (auto Projectile : StandardMissilePool) if (IsValid(Projectile)) Projectile->Destroy();
    for (auto Projectile : SpecialWeaponPool) if (IsValid(Projectile)) Projectile->Destroy();
    }
    Super::EndPlay(Reason);
}
