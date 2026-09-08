#include "MissileWeaponComponent.h"

#include "MissileProjectile.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UMissileWeaponComponent::UMissileWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMissileWeaponComponent::Configure(
	const TArray<USceneComponent*>& NewHardpoints
)
{
	MissileHardpoints = NewHardpoints;
	bHardpointLoaded.Init(true, MissileHardpoints.Num());
	MissileReloadTimerHandles.SetNum(MissileHardpoints.Num());
	RemainingSpecialAmmo = SpecialWeaponMaximumAmmo;
	NextSpecialHardpoint = 0;
}

void UMissileWeaponComponent::InitializeMissilePool()
{
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
	Projectile->ActivateProjectile(
		Hardpoint->GetComponentLocation(),
		Hardpoint->GetComponentRotation(),
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
	if (NewMode == ESecondaryWeaponMode::SpecialWeapon && !SpecialProjectileClass)
	{
		WeaponMode = ESecondaryWeaponMode::StandardMissile;
		return;
	}

	WeaponMode = NewMode;
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
		return RemainingSpecialAmmo > 0;
	}

	return bHardpointLoaded.Contains(true);
}

bool UMissileWeaponComponent::HasMissileInTheAir() const
{
	return ActiveProjectileCount > 0;
}

float UMissileWeaponComponent::GetEffectiveRange() const
{
	return EffectiveMissileRange;
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
