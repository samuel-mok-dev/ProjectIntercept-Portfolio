#include "MissileProjectile.h"
#include "../Tests/FactionBalanceTelemetry.h"
// Fill out your copyright notice in the Description page of Project Settings.


#include "Net/UnrealNetwork.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "ShipBase.h"
#include "CountermeasureComponent.h"
#include "DefenseTurretBase.h"
#include "CapitalShipBase.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
AMissileProjectile::AMissileProjectile()
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

	CollisionComponent->SetSphereRadius(5.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
    CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
    CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);

	MissileEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MissileEffect"));
	MissileEffect->SetupAttachment(CollisionComponent);
    MissileEffect->SetCanEverAffectNavigation(false);
    CollisionComponent->SetCanEverAffectNavigation(false);
	MissileEffect->SetAutoActivate(false);

	MissileAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MissileAudioComponent"));
	MissileAudioComponent->SetupAttachment(CollisionComponent);
	MissileAudioComponent->bAutoActivate = false;

	MissileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissileMesh"));
	MissileMesh->SetupAttachment(CollisionComponent);
    MotorGlow=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MotorGlow"));
    MotorGlow->SetupAttachment(CollisionComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GlowMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> GlowMaterial(TEXT("/Game/FX/Fleet/M_MissileMotor.M_MissileMotor"));
    MotorGlow->SetStaticMesh(GlowMesh.Object);
    if(GlowMaterial.Succeeded())MotorGlow->SetMaterial(0,GlowMaterial.Get());
    MotorGlow->SetRelativeScale3D(FVector(.12f,.022f,.022f));
    MotorGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);MotorGlow->SetCanEverAffectNavigation(false);
    MotorGlow->SetCastShadow(false);MotorGlow->bReceivesDecals=false;MotorGlow->SetCullDistance(150000);MotorGlow->SetVisibility(false);

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
    if (!HasAuthority() || !bIsActive) return;
    TRACE_CPUPROFILER_EVENT_SCOPE(SpaceAce_AMissileProjectile_Tick);
	Super::Tick(DeltaTime);

    if (bIsHomingMissile && IsValid(HomingTarget))
        if (const auto* Decoys=HomingTarget->FindComponentByClass<UCountermeasureComponent>(); Decoys && Decoys->IsDefeatingGuidance())
        {
            SetTarget(nullptr);
            ForceNetUpdate();
        }
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
					HomingTurnRate * GuidanceResponsiveness
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
			Detonate(HitActor,HitResult.ImpactPoint);
			return;
		}

        Detonate(nullptr,HitResult.ImpactPoint);
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

void AMissileProjectile::ConfigurePayload(float TurnRate, float TrackingAlignment, float Radius, float CapitalDamageMultiplier)
{
    HomingTurnRate=FMath::Max(0.0f,TurnRate);
    MinimumTrackingAlignment=FMath::Clamp(TrackingAlignment,-1.0f,1.0f);
    BlastRadius=FMath::Max(0.0f,Radius);
    AntiShipDamageMultiplier=FMath::Max(1.0f,CapitalDamageMultiplier);
    CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
    CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
}

void AMissileProjectile::Detonate(AActor* DirectHit,const FVector& Location)
{
    if (!HasAuthority() || !bIsActive) return;
#if WITH_DEV_AUTOMATION_TESTS
    if(GetClass()->GetName().Contains(TEXT("UnguidedBomb")))
        UE_LOG(LogTemp,Display,TEXT("UGB_IMPACT owner=%s actor=%s class=%s"),*GetNameSafe(GetOwner()),*GetNameSafe(DirectHit),DirectHit?*DirectHit->GetClass()->GetName():TEXT("None"));
#endif
    TSet<AActor*> Victims;
    if (IsValid(DirectHit)) Victims.Add(DirectHit);
    if (BlastRadius>0)
    {
        TArray<FOverlapResult> Overlaps;
        FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SpecialWeaponBlast),false,GetOwner());
        GetWorld()->OverlapMultiByObjectType(Overlaps,Location,FQuat::Identity,Objects,FCollisionShape::MakeSphere(BlastRadius),Query);
        for (const auto& Hit : Overlaps)
            if (AActor* Actor=Hit.GetActor(); Actor && (Actor->IsA<AShipBase>() || Actor->IsA<ADefenseTurretBase>() || Actor->IsA<ACapitalShipBase>())) Victims.Add(Actor);
    }
    for (AActor* Victim : Victims)
    {
        if (!IsValid(Victim) || Victim==GetOwner()) continue;
        if (Victim!=DirectHit)
        {
            FHitResult Block;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(SpecialWeaponBlastVisibility),false,GetOwner());
            if (GetWorld()->LineTraceSingleByChannel(Block,Location-GetActorForwardVector()*20.0f,
                Victim->GetActorLocation(),ECC_Visibility,Query) && Block.GetActor()!=Victim) continue;
        }
        const bool bCapital=Victim->IsA<ADefenseTurretBase>() || Victim->IsA<ACapitalShipBase>();
        const float Falloff=Victim==DirectHit || BlastRadius<=0 ? 1.0f :
            FMath::Clamp(1.0f-FVector::Distance(Location,Victim->GetActorLocation())/BlastRadius,.25f,1.0f);
#if WITH_DEV_AUTOMATION_TESTS
        FactionBalance::Hit(GetOwner(), Victim, true);
#endif
        UGameplayStatics::ApplyDamage(Victim,Damage*Falloff*(bCapital?AntiShipDamageMultiplier:1.0f),GetInstigatorController(),this,nullptr);
    }
    MulticastImpact(Location);DeactivateProjectile();
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
    ClearWarningTarget();
    if (HasAuthority()) HomingTarget = nullptr;
	
	WarnedTarget.Reset();
	bWarningRegistered = false;

	ActiveTime = 0.0f;
	
	SetActorLocation(SpawnLocation);
	SetActorRotation(SpawnRotation);
	SetOwner(NewOwner);
#if WITH_DEV_AUTOMATION_TESTS
    if (HasAuthority()) FactionBalance::Shot(NewOwner, true);
#endif
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
	SetActorEnableCollision(HasAuthority());
	SetActorTickEnabled(true);

    if(MotorGlow)
    {
        const auto* Mesh=MissileMesh?MissileMesh->GetStaticMesh().Get():nullptr;
        const bool bMotor=MissileEffect && MissileEffect->GetAsset() && Mesh && GetNetMode()!=NM_DedicatedServer;
        if(bMotor)
        {
            const auto Bounds=Mesh->GetBounds();
            MotorGlow->SetRelativeLocation(FVector((Bounds.Origin.X-Bounds.BoxExtent.X)*MissileMesh->GetRelativeScale3D().X-5.5f,0,0));
        }
        MotorGlow->SetVisibility(bMotor);
    }

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
    if (HasAuthority()) PublishNetState();
}

void AMissileProjectile::DeactivateProjectile()
{
	if(MotorGlow)MotorGlow->SetVisibility(false);
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

    // Keep the pool owner for relevancy, cleanup, and consistent ownership.

	if (HasAuthority() && bWasActive)
	{
        PublishNetState();
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

void AMissileProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMissileProjectile, NetState);
    DOREPLIFETIME(AMissileProjectile, HomingTarget);
}
void AMissileProjectile::PublishNetState()
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
void AMissileProjectile::OnRep_NetState() { ApplyNetState(NetState); }
void AMissileProjectile::MulticastProjectileState_Implementation(const FProjectileNetState& State)
{
    if (!HasAuthority()) ApplyNetState(State);
}
void AMissileProjectile::ApplyNetState(const FProjectileNetState& State)
{
    if (HasAuthority() || int32(State.Revision - AppliedRevision) <= 0) return;
    AppliedRevision = State.Revision;
    if (State.bActive)
    {
        Speed = State.Speed;
        ActivateProjectile(State.Location, State.Rotation, GetOwner(), 0.0f);
        OnRep_HomingTarget();
    }
    else DeactivateProjectile();
}

void AMissileProjectile::OnRep_HomingTarget()
{
    ClearWarningTarget();
    if (!bIsActive) return;
    if (AShipBase* TargetShip = Cast<AShipBase>(HomingTarget))
    {
        TargetShip->RegisterIncomingMissile(this);
        WarnedTarget = TargetShip;
        bWarningRegistered = true;
    }
}
void AMissileProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearWarningTarget();
    Super::EndPlay(Reason);
}
void AMissileProjectile::MulticastImpact_Implementation(FVector_NetQuantize Location)
{
    if (GetNetMode() == NM_DedicatedServer) return;
    if (MissileExplosionSound) UGameplayStatics::PlaySoundAtLocation(this, MissileExplosionSound, Location);
    if (MissileExplosionEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), MissileExplosionEffect, Location, FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
}
