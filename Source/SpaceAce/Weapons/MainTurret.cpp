#include "MainTurret.h"

#include "LaserProjectile.h"
#include "ShipBase.h"
#include "Components/SceneComponent.h"

void AMainTurret::FireAtTarget(AShipBase* Target)
{
	if (!ProjectileClass || !Target) return;

	const USceneComponent* Muzzles[] = { GetPrimaryMuzzle(), GetSecondaryMuzzle() };
	for (const USceneComponent* Muzzle : Muzzles)
	{
		if (!Muzzle) continue;
        if (Muzzle == GetSecondaryMuzzle() && !bFireSecondaryMuzzle) continue;
		const FVector Location = Muzzle->GetComponentLocation();
		const FRotator Rotation = Muzzle->GetComponentRotation();
		ALaserProjectile* Projectile = GetWorld()->SpawnActor<ALaserProjectile>(
			ProjectileClass, Location, Rotation);
		if (Projectile)
		{
			Projectile->SetLifeSpan(5.f);
            Projectile->ActivateProjectile(Location, Rotation, this,
				ProjectileDamage, ProjectileSpeed);
		}
	}
}
