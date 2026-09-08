#include "LaserWeaponComponent.h"
#include "LaserProjectile.h"

ULaserWeaponComponent::ULaserWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Disable ticking for this component
}

void ULaserWeaponComponent::Configure(
    const TArray<USceneComponent*>& NewCannonMuzzles,
    float NewDamageMultiplier,
    float NewSpeedMultiplier,
    float NewGunFireRate,
    int32 NewLaserPoolSize,
    TSubclassOf<ALaserProjectile> NewLaserProjectileClass,
    UMaterialInterface* NewLaserMaterial
)
{
    CannonMuzzles = NewCannonMuzzles;
    DamageMultiplier = NewDamageMultiplier;
    SpeedMultiplier = NewSpeedMultiplier;
    GunFireRate = NewGunFireRate;
    LaserPoolSize = NewLaserPoolSize;
    LaserProjectileClass = NewLaserProjectileClass;
    LaserMaterial = NewLaserMaterial;
}

// Initialize the laser pool with pre-spawned laser projectiles
void ULaserWeaponComponent::InitializeLaserPool()
{
    if (!LaserProjectileClass)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s: LaserProjectileClass is not assigned."),
            *GetNameSafe(GetOwner())
        );

        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s: Cannot initialize laser pool because World is null."),
            *GetNameSafe(GetOwner())
        );

        return;
    }


    // Empty the availability queue before rebuilding it.
    LaserPool.Empty();

    ALaserProjectile* QueuedLaser = nullptr;

    while (AvailableLasers.Dequeue(QueuedLaser))
    {
        // Empty the availability queue before rebuilding it.
    }
        
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = Cast<APawn>(GetOwner());
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (int32 i = 0; i < LaserPoolSize; ++i)
    {
        ALaserProjectile* Laser =
            World->SpawnActor<ALaserProjectile>(
                LaserProjectileClass,
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                SpawnParams
            );

        if (!Laser)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("%s: Failed to spawn laser projectile %d of %d."),
                *GetNameSafe(GetOwner()),
                i + 1,
                LaserPoolSize
            );

            continue;
        }

        Laser->OnLaserDeactivated.BindUObject(
            this,
            &ULaserWeaponComponent::ReturnLaserToPool
        );

        if (LaserMaterial)
        {
            Laser->SetLaserMaterial(LaserMaterial);
        }

        Laser->DeactivateProjectile();

        LaserPool.Add(Laser);
        AvailableLasers.Enqueue(Laser);
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("%s: Initialized laser pool with %d projectiles."),
        *GetNameSafe(GetOwner()),
        LaserPool.Num()
    );
}

ALaserProjectile* ULaserWeaponComponent::GetAvailableLaser()
{
    // Get laser from queue and activate it
    ALaserProjectile* Laser = nullptr;

    while (AvailableLasers.Dequeue(Laser))
    {
        if (IsValid(Laser) && Laser->IsAvailable())
        {
            return Laser;
        }
    }    

    return nullptr; // No available lasers in the pool
}

void ULaserWeaponComponent::FireLaser()
{
    for (USceneComponent* Muzzle : CannonMuzzles)
    {
        ALaserProjectile* Laser = GetAvailableLaser();
        
        if (!Laser)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("%s: No available lasers to fire from pool!"),
                *GetNameSafe(GetOwner())
            );
            continue;
        }
        
        float ActualDamage = LaserDamage * DamageMultiplier;
        float ActualSpeed = LaserSpeed * SpeedMultiplier;
        
        Laser->ActivateProjectile(
            Muzzle->GetComponentLocation(),
            Muzzle->GetComponentRotation(),
            GetOwner(),
            ActualDamage,
            ActualSpeed
        );

        OnLaserFired.Broadcast();
    }
}

void ULaserWeaponComponent::StartFiringLasers()
{
    if (bIsFiring)
    {
        return; 
    }

    bIsFiring = true;
    
    FireLaser();

    const float SafeFireRateMultiplier =
        FMath::Max(GunFireRate, 0.01f);

    const float FireInterval =
        BaseGunFireInterval / SafeFireRateMultiplier;

    GetWorld()->GetTimerManager().SetTimer(
        FireTimerHandle,
        this,
        &ULaserWeaponComponent::FireLaser,
        FireInterval,
        true
    );
}

void ULaserWeaponComponent::StopFiringLasers()
{
    if (!bIsFiring)
    {
        return; // Not firing, do nothing
    }

    bIsFiring = false;
    
    // Stop firing lasers when button is released
    GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void ULaserWeaponComponent::ReturnLaserToPool(ALaserProjectile* Laser)
{
    if (!IsValid(Laser))
    {
        return;
    }

    AvailableLasers.Enqueue(Laser);
    
}

float ULaserWeaponComponent::GetEffectiveRange() const
{
    const float ActualSpeed = LaserSpeed * SpeedMultiplier;
    return ActualSpeed * LaserLifetime;
}

float ULaserWeaponComponent::GetProjectileSpeed() const
{
    return LaserSpeed * SpeedMultiplier;
}