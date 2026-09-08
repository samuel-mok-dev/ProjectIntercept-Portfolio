// Fill out your copyright notice in the Description page of Project Settings.


#include "MissileProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "ShipBase.h"

// Sets default values
AMissileProjectile::AMissileProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	
	SetRootComponent(CollisionComponent);

	CollisionComponent->SetSphereRadius(5.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);

	MissileEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MissileEffect"));
	MissileEffect->SetupAttachment(CollisionComponent);
	MissileEffect->SetAutoActivate(false);

	MissileAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MissileAudioComponent"));
	MissileAudioComponent->SetupAttachment(CollisionComponent);
	MissileAudioComponent->bAutoActivate = false;

	MissileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissileMesh"));
	MissileMesh->SetupAttachment(CollisionComponent);

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

// Called when the game starts or when spawned
void AMissileProjectile::BeginPlay()
{
	Super::BeginPlay();

	MissileAudioComponent->OnAudioFinished.AddDynamic(
		this, 
		&AMissileProjectile::OnLaunchSoundFinished
	);
}

// Called every frame
void AMissileProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsHomingMissile && IsValid(HomingTarget))
	{
		const FVector CurrentLocation =
			GetActorLocation();

		const FVector DirectionToTarget =
			(
				HomingTarget->GetActorLocation() -
				CurrentLocation
			).GetSafeNormal();

		const FVector CurrentForward =
			GetActorForwardVector();

		const float TargetAlignment =
			FVector::DotProduct(
				CurrentForward,
				DirectionToTarget
			);

		if (TargetAlignment >= MinimumTrackingAlignment)
		{
			const FVector NewDirection =
				FMath::VInterpConstantTo(
					CurrentForward,
					DirectionToTarget,
					DeltaTime,
					HomingTurnRate
				).GetSafeNormal();

			SetActorRotation(
				NewDirection.Rotation()
			);

			CurrentVelocity =
				NewDirection * Speed + InheritedVelocity;
		}
		else
		{
			HomingTarget = nullptr;
			ClearWarningTarget();
		}
	}

	if (!FMath::IsNearlyZero(GravityScale))
	{
		CurrentVelocity.Z +=
			GetWorld()->GetGravityZ() * GravityScale * DeltaTime;
	}

	const FVector Movement = CurrentVelocity * DeltaTime;

	FHitResult HitResult;
	
	AddActorWorldOffset(Movement, true, &HitResult);

	ActiveTime += DeltaTime;

	if (HitResult.bBlockingHit)
	{
		UE_LOG(
        LogTemp,
        Warning,
        TEXT("Missile impact | Explosion sound: %s"),
        *GetNameSafe(MissileExplosionSound)
    );
		
		if (AActor* HitActor = HitResult.GetActor())
		{
			UGameplayStatics::ApplyDamage(HitActor, Damage, GetInstigatorController(), this, nullptr);
			
			if (MissileExplosionSound)
			{
				UGameplayStatics::PlaySoundAtLocation(
					this,
					MissileExplosionSound,
					HitResult.ImpactPoint
				);
			}
			
			if (MissileExplosionEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					GetWorld(),
					MissileExplosionEffect,
					HitResult.ImpactPoint,
					FRotator::ZeroRotator
				);
			}
			
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Missile HIT target: %s | Damage: %.1f | Flight time: %.2fs"),
				*HitActor->GetName(),
				Damage,
				ActiveTime
			);
		}

		DeactivateProjectile();
		return;
	}

	if (ActiveTime >= MaxLifetime)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Missile EXPIRED after %.2fs (max: %.2fs) | Had target: %s"),
			ActiveTime,
			MaxLifetime,
			HomingTarget ? TEXT("Yes") : TEXT("No")
		);
		
		DeactivateProjectile();
		return;
	}
}

void AMissileProjectile::ConfigureFlight(
	bool bNewIsHomingMissile,
	const FVector& NewInheritedVelocity,
	float NewGravityScale
)
{
	bIsHomingMissile = bNewIsHomingMissile;
	InheritedVelocity = NewInheritedVelocity;
	GravityScale = NewGravityScale;
}

void AMissileProjectile::InitializeProjectile(
	float NewSpeed,
	float NewDamage,
	float NewLifetime
)
{
	Speed = NewSpeed;
	Damage = NewDamage;
	MaxLifetime = NewLifetime;
}

void AMissileProjectile::ActivateProjectile(
	const FVector& SpawnLocation, 
	const FRotator& SpawnRotation,
	AActor* NewOwner,
	float NewDamage
)
{
	HomingTarget = nullptr;
	
	WarnedTarget.Reset();
	bWarningRegistered = false;

	ActiveTime = 0.0f;
	
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
	CurrentVelocity =
		GetActorForwardVector() * Speed + InheritedVelocity;

	if (CollisionComponent && IsValid(NewOwner))
	{
		CollisionComponent->IgnoreActorWhenMoving(NewOwner, true);
	}
	
	Damage = NewDamage;

	bIsActive = true;

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	if (MissileEffect)
	{
		MissileEffect->ReinitializeSystem();
		MissileEffect->Activate();
	}

	if (MissileAudioComponent && MissileLaunchSound)
	{
		MissileAudioComponent->SetSound(MissileLaunchSound);
		MissileAudioComponent->Play();
	}
}

void AMissileProjectile::DeactivateProjectile()
{
	ClearWarningTarget();

	const bool bWasActive = bIsActive;
	
	bIsActive = false;
	ActiveTime = 0.0f;
	HomingTarget = nullptr;
	CurrentVelocity = FVector::ZeroVector;
	InheritedVelocity = FVector::ZeroVector;
	GravityScale = 0.0f;

	if (MissileEffect)
	{
		MissileEffect->DeactivateImmediate();
	}

	if (MissileAudioComponent)
    {
        MissileAudioComponent->Stop();
    }

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);

	SetOwner(nullptr);

	if (bWasActive)
	{
		OnMissileDeactivated.ExecuteIfBound(this);
	}
}

// Set the homing target for this missile
void AMissileProjectile::SetTarget(AActor* NewTarget)
{
	if (!bIsHomingMissile)
	{
		HomingTarget = nullptr;
		ClearWarningTarget();
		return;
	}

	if (bWarningRegistered)
	{
		if (AShipBase* PreviousTarget = WarnedTarget.Get())
		{
			PreviousTarget->UnregisterIncomingMissile(this);
		}

		bWarningRegistered = false;
		WarnedTarget.Reset();
	}

	HomingTarget = NewTarget;

	AShipBase* TargetShip = Cast<AShipBase>(NewTarget);

	if (IsValid(TargetShip))
	{
		TargetShip->RegisterIncomingMissile(this);
		WarnedTarget = TargetShip;
		bWarningRegistered = true;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Missile tracking and warning %s"),
			*GetNameSafe(TargetShip)
		);
	}
	
	if (HomingTarget)
	{
		float DistanceToTarget = FVector::Distance(
			GetActorLocation(), 
			HomingTarget->GetActorLocation()
		);
		
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Missile NOW TRACKING target: %s | Distance: %.1f units | Turn rate: %.1f deg/s"),
			*HomingTarget->GetName(),
			DistanceToTarget,
			HomingTurnRate
		);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Missile target CLEARED - flying dumb-fire")
		);
	}
}

void AMissileProjectile::ClearWarningTarget()
{
	if (!bWarningRegistered)
	{
		return;
	}

	if (AShipBase* PreviousTarget = WarnedTarget.Get())
	{
		PreviousTarget->UnregisterIncomingMissile(this);
	}

		bWarningRegistered = false;
		WarnedTarget.Reset();
}

void AMissileProjectile::OnLaunchSoundFinished()
{
	if (!bIsActive)
	{
		return;
	}
	
	if (MissileAudioComponent && MissileMotorSound)
	{
		MissileAudioComponent->SetSound(MissileMotorSound);
		MissileAudioComponent->Play();
	}
}
