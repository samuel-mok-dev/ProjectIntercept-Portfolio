#include "LaserProjectile.h"
#include "../Tests/FactionBalanceTelemetry.h"
// Fill out your copyright notice in the Description page of Project Settings.


#include "LaserWeaponComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"

// Sets default values
ALaserProjectile::ALaserProjectile()
{
    bReplicates = true;
    bNetUseOwnerRelevancy = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(60.0f);
    SetMinNetUpdateFrequency(30.0f);
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	
	SetRootComponent(CollisionComponent);

	CollisionComponent->InitSphereRadius(100.0f);
    CollisionComponent->SetCanEverAffectNavigation(false);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
	CollisionComponent->OnComponentHit.AddDynamic(this, &ALaserProjectile::HandleHit);

	LaserEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LaserEffect"));
	LaserEffect->SetupAttachment(CollisionComponent);
	LaserEffect->SetAutoActivate(false);
    LaserEffect->SetCanEverAffectNavigation(false);

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);

	// Initialize ProjectileMovementComponent
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;

	// Fire actor along actor's forward vector
	ProjectileMovement->Velocity = FVector(1.0f, 0.0f, 0.0f);

	ProjectileMovement->ProjectileGravityScale = 0.0f; // No gravity
	ProjectileMovement->bRotationFollowsVelocity = true; // Rotate to match velocity direction
	ProjectileMovement->bShouldBounce = false; // No bouncing

	ProjectileMovement->bAutoActivate = false; // Don't activate until projectile is fired
}

// Called when the game starts or when spawned
void ALaserProjectile::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ALaserProjectile::Tick(float DeltaTime)
{
    if (!HasAuthority() || !bIsActive) return;
    Super::Tick(DeltaTime);

    if (!bIsActive)
    {
        return;
    }

    ActiveTime += DeltaTime;

    if (ActiveTime >= MaxLifetime)
    {
        DeactivateProjectile();
    }
}

void ALaserProjectile::InitializeProjectile(float NewSpeed, float NewDamage, float NewLifetime)
{
    Speed = FMath::IsFinite(NewSpeed) ? FMath::Max(0.0f, NewSpeed) : 0.0f;
    Damage = FMath::IsFinite(NewDamage) ? FMath::Max(0.0f, NewDamage) : 0.0f;
    MaxLifetime = FMath::IsFinite(NewLifetime) ? FMath::Max(0.01f, NewLifetime) : 1.0f;
}

void ALaserProjectile::ActivateProjectile(
	const FVector& SpawnLocation, 
	const FRotator& SpawnRotation,
	AActor* NewOwner,
	float NewDamage,
	float NewSpeed
	)
{
	SetActorLocation(SpawnLocation);
	SetActorRotation(SpawnRotation);
	SetOwner(NewOwner);
#if WITH_DEV_AUTOMATION_TESTS
    if (HasAuthority()) FactionBalance::Shot(NewOwner, false);
#endif

	if (CollisionComponent && IsValid(NewOwner))
	{
		CollisionComponent->IgnoreActorWhenMoving(NewOwner, true);
		if (AActor* ParentActor = NewOwner->GetOwner())
		{
			CollisionComponent->IgnoreActorWhenMoving(ParentActor, true);
		}
	}

	Damage = NewDamage;
	Speed = NewSpeed;

	bIsActive = true;
	ActiveTime = 0.0f;

	SetActorHiddenInGame(false);
	SetActorEnableCollision(HasAuthority());
	SetActorTickEnabled(true);

	if (ProjectileMovement)
	{
		ProjectileMovement->SetUpdatedComponent(CollisionComponent);

		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;

		ProjectileMovement->Velocity =
			GetActorForwardVector() * Speed;

		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->Activate(true);

		ProjectileMovement->UpdateComponentVelocity();
	}

	if (LaserEffect && GetNetMode() != NM_DedicatedServer)
	{

		if (IsValid(LaserMaterial))
		{
			LaserEffect->SetVariableMaterial(
				FName("LaserMaterial"),
				LaserMaterial
			);
		}

		LaserEffect->Activate(true); // Reset the pooled effect once, after applying its material.
	}

	if (LaserFiredSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			LaserFiredSound,
			GetActorLocation()
		);
	}
    if (HasAuthority()) PublishNetState();
}

void ALaserProjectile::DeactivateProjectile()
{
	const bool bWasActive = bIsActive;
	
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	ProjectileMovement->SetComponentTickEnabled(false);
	
	bIsActive = false;
	ActiveTime = 0.0f;

	if (LaserEffect)
	{
		LaserEffect->DeactivateImmediate();
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);

    // Keep the pool owner for relevancy, cleanup, and consistent ownership.

	if (HasAuthority() && bWasActive)
	{
        PublishNetState();
		OnLaserDeactivated.ExecuteIfBound(this);
	}
}

void ALaserProjectile::HandleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	if (!HasAuthority() || !bIsActive || !IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

    MulticastImpact(Hit.ImpactPoint);

#if WITH_DEV_AUTOMATION_TESTS
    FactionBalance::Hit(GetOwner(), OtherActor, false);
#endif
	UGameplayStatics::ApplyDamage(
		OtherActor,
		Damage,
		GetInstigatorController(),
		this,
		nullptr
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Laser HIT: %s"),
		*GetNameSafe(OtherActor)
	);

	DeactivateProjectile();
}

void ALaserProjectile::SetLaserMaterial(UMaterialInterface* NewMaterial)
{
	LaserMaterial = NewMaterial;

	if (LaserEffect && LaserMaterial)
	{
		LaserEffect->SetVariableMaterial(
			FName("LaserMaterial"),
			LaserMaterial
		);
	}
}

void ALaserProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALaserProjectile, NetState);
    DOREPLIFETIME(ALaserProjectile, LaserMaterial);
}
void ALaserProjectile::PublishNetState()
{
    ++NetState.Revision;
    NetState.bActive = bIsActive;
    NetState.Location = GetActorLocation();
    NetState.Rotation = GetActorRotation();
    NetState.Speed = Speed;
    ForceNetUpdate();
    // A short-lived shot can activate and hit between property updates.
    MulticastProjectileState(NetState);
}
void ALaserProjectile::OnRep_NetState() { ApplyNetState(NetState); }
void ALaserProjectile::MulticastProjectileState_Implementation(const FProjectileNetState& State)
{
    if (!HasAuthority()) ApplyNetState(State);
}
void ALaserProjectile::ApplyNetState(const FProjectileNetState& State)
{
    if (HasAuthority() || int32(State.Revision - AppliedRevision) <= 0) return;
    AppliedRevision = State.Revision;
    if (State.bActive)
    {
        Speed = State.Speed;
        ActivateProjectile(State.Location, State.Rotation, GetOwner(), 0.0f, State.Speed);
        if (GetOwner())
            if (auto* Weapon = GetOwner()->FindComponentByClass<ULaserWeaponComponent>()) Weapon->OnLaserFired.Broadcast();
    }
    else DeactivateProjectile();
}

void ALaserProjectile::OnRep_LaserMaterial() { SetLaserMaterial(LaserMaterial); }
void ALaserProjectile::MulticastImpact_Implementation(FVector_NetQuantize Location)
{
    if (GetNetMode() == NM_DedicatedServer) return;
    if (HitSparkSound) UGameplayStatics::PlaySoundAtLocation(this, HitSparkSound, Location);
    if (HitSparkEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitSparkEffect, Location, FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
}
