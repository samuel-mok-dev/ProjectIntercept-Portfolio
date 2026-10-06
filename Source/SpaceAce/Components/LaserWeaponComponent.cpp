#include "LaserWeaponComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "LaserProjectile.h"
#include "ShipBase.h"
#include "ShipAIController.h"

FVector ULaserWeaponComponent::PredictIntercept(const FVector& Origin, const FVector& TargetPosition,
    const FVector& TargetVelocity, float ProjectileSpeed, float MaxFlightTime)
{
    const FVector Relative = TargetPosition - Origin;
    const double A = TargetVelocity.SizeSquared() - FMath::Square(double(ProjectileSpeed));
    const double B = 2.0 * FVector::DotProduct(Relative, TargetVelocity);
    const double C = Relative.SizeSquared();
    double Time = -1.0;
    if (ProjectileSpeed <= 0 || MaxFlightTime <= 0) return TargetPosition;
    if (FMath::Abs(A) < 0.001)
    {
        if (FMath::Abs(B) > 0.001) Time = -C / B;
    }
    else
    {
        const double Discriminant = B * B - 4.0 * A * C;
        if (Discriminant >= 0)
        {
            const double Root = FMath::Sqrt(Discriminant);
            const double T1 = (-B - Root) / (2.0 * A);
            const double T2 = (-B + Root) / (2.0 * A);
            if (T1 >= 0) Time = T1;
            if (T2 >= 0 && (Time < 0 || T2 < Time)) Time = T2;
        }
    }
    // Unreachable contacts do not produce an unbounded lead point.
    return TargetPosition + TargetVelocity * FMath::Clamp(Time, 0.0, double(MaxFlightTime));
}

FVector ULaserWeaponComponent::ConvergedDirection(const FVector& MuzzleOrigin,
    const FVector& AimPoint, const FVector& ShipForward)
{
    const FVector Forward = ShipForward.GetSafeNormal();
    const FVector Desired = (AimPoint - MuzzleOrigin).GetSafeNormal();
    const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, Desired), -1.0, 1.0));
    constexpr double MaxAngle = 2.0 * UE_PI / 180.0;
    if (Desired.IsNearlyZero() || Forward.IsNearlyZero()) return Forward;
    if (Angle <= MaxAngle) return Desired;
    const FVector Axis = FVector::CrossProduct(Forward, Desired).GetSafeNormal();
    return Axis.IsNearlyZero() ? Forward : FQuat(Axis, MaxAngle).RotateVector(Forward);
}

FVector ULaserWeaponComponent::GetGunAimPoint() const
{
    const auto* Ship = Cast<AShipBase>(GetOwner());
    if (!Ship) return GetOwner() ? GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 30000.f : FVector::ZeroVector;
    if (Cast<AShipAIController>(Ship->GetController()))
    {
        if (const AActor* Target = Ship->GetCurrentTarget(); IsValid(Target))
        {
            const FVector Aim = PredictIntercept(Ship->GetActorLocation(), Target->GetActorLocation(),
                Target->GetVelocity(), GetProjectileSpeed(), LaserLifetime);
            const double Range = FVector::Distance(Ship->GetActorLocation(), Aim);
            return Aim + Range * (Ship->GetActorRightVector()*FMath::Tan(FMath::DegreesToRadians(AIAimErrorDegrees.X)) +
                Ship->GetActorUpVector()*FMath::Tan(FMath::DegreesToRadians(AIAimErrorDegrees.Y)));
        }
    }
    // Player convergence follows the bore, never a selected off-axis target or free-look camera.
    return Ship->GetActorLocation() + Ship->GetShipForwardVector() * 30000.f;
}

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
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_ULaserWeaponComponent_InitializeLaserPool);
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
    for (auto Projectile : LaserPool) if (IsValid(Projectile)) Projectile->Destroy();
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

        Laser->SetReplicates(true);
        Laser->SetReplicateMovement(true);
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
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_ULaserWeaponComponent_FireLaser);
    if (!GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now + KINDA_SMALL_NUMBER < NextFireTime)
    {
        // An early callback or a quick re-press waits out the remaining cooldown.
        // Never discard a shot and wait an additional whole firing interval.
        ScheduleNextShot();
        return;
    }
    const double Interval = BaseGunFireInterval / FMath::Max(GunFireRate, 0.01f);
    NextFireTime += Interval;
    // Preserve cadence across ordinary frame jitter, but do not burst after a hitch.
    if (NextFireTime <= Now) NextFireTime = Now + Interval;
    const FVector AimPoint = GetGunAimPoint();
    const auto* Ship = Cast<AShipBase>(GetOwner());
    for (USceneComponent* Muzzle : CannonMuzzles)
    {
        if (!IsValid(Muzzle)) continue;
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
            Ship ? ConvergedDirection(Muzzle->GetComponentLocation(), AimPoint,
                Ship->GetShipForwardVector()).Rotation() : Muzzle->GetComponentRotation(),
            GetOwner(),
            ActualDamage,
            ActualSpeed
        );

        OnLaserFired.Broadcast();
    }
    ScheduleNextShot();
}

void ULaserWeaponComponent::ScheduleNextShot()
{
    if (!bIsFiring || !GetWorld()) return;
    const float Delay = FMath::Max(float(NextFireTime - GetWorld()->GetTimeSeconds()), KINDA_SMALL_NUMBER);
    GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this,
        &ULaserWeaponComponent::FireLaser, Delay, false);
}

void ULaserWeaponComponent::StartFiringLasers()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) return;
    if (bIsFiring)
    {
        return; 
    }

    bIsFiring = true;
    
    FireLaser();
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
void ULaserWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    if (GetOwner() && GetOwner()->HasAuthority())
    {
    for (auto Projectile : LaserPool) if (IsValid(Projectile)) Projectile->Destroy();
    }
    Super::EndPlay(Reason);
}
