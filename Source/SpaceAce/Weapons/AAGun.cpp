#include "AAGun.h"

#include "ShipBase.h"
#include "LaserProjectile.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AAAGun::AAAGun()
{
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Navy(TEXT("/Game/materials/M_LaserBlue.M_LaserBlue"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Imperial(TEXT("/Game/materials/M_LaserRed.M_LaserRed"));
    RoyalNavyLaserMaterial = Navy.Object;
    ImperialLaserMaterial = Imperial.Object;
}

void AAAGun::FireAtTarget(AShipBase* Target)
{
    if (!HasAuthority() || !ProjectileClass || !Target) return;

    const USceneComponent* Muzzles[] = { GetPrimaryMuzzle(), GetSecondaryMuzzle() };
    for (const USceneComponent* Muzzle : Muzzles)
    {
        if (!Muzzle) continue;
        const FVector Location = Muzzle->GetComponentLocation();
        const FRotator Rotation = Muzzle->GetComponentRotation();
        ALaserProjectile* Projectile = GetWorld()->SpawnActor<ALaserProjectile>(
            ProjectileClass, Location, Rotation);
        if (Projectile)
        {
            Projectile->SetLifeSpan(5.f);
            // Choose from the firing turret's current faction, not the local player's team.
            // Set before activation so both the initial effect and replicated shots agree.
            if (GetTeamID() == 0) Projectile->SetLaserMaterial(RoyalNavyLaserMaterial);
            else if (GetTeamID() == 1) Projectile->SetLaserMaterial(ImperialLaserMaterial);
            Projectile->ActivateProjectile(Location, Rotation, this,
                ProjectileDamage, ProjectileSpeed);
        }
    }
}
