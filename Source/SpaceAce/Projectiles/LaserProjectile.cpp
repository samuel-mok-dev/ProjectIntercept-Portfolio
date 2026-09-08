// Fill out your copyright notice in the Description page of Project Settings.


#include "LaserProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"

// Sets default values
ALaserProjectile::ALaserProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	
	SetRootComponent(CollisionComponent);

	CollisionComponent->InitSphereRadius(100.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
	CollisionComponent->OnComponentHit.AddDynamic(this, &ALaserProjectile::HandleHit);

	LaserEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LaserEffect"));
	LaserEffect->SetupAttachment(CollisionComponent);
	LaserEffect->SetAutoActivate(false);

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
	SetActorEnableCollision(true);
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

	if (LaserEffect)
	{
		LaserEffect->ReinitializeSystem();

		if (IsValid(LaserMaterial))
		{
			LaserEffect->SetVariableMaterial(
				FName("LaserMaterial"),
				LaserMaterial
			);
		}

		LaserEffect->Activate();
	}

	if (LaserFiredSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			LaserFiredSound,
			GetActorLocation()
		);
	}
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

	SetOwner(nullptr);

	if (bWasActive)
	{
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
	if (!bIsActive || !IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	if (HitSparkSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			HitSparkSound,
			Hit.ImpactPoint
		);
	}
	
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			HitSparkEffect,
			Hit.ImpactPoint,
			FRotator::ZeroRotator
		);

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
