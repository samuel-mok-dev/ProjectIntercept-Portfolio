#include "PlayerShip.h"
#include "SpaceAceUserSettings.h"

#include "HealthComponent.h"
#include "TargetingComponent.h"
#include "LaserWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "AvionicsSynthComponent.h"
#include "AudioVoiceWarningComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "FlightCameraShakes.h"
#include "MissileProjectile.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/PlayerCameraManager.h"

APlayerShip::APlayerShip()
{
    // Space-flight separation quickly exceeds the default 150 m actor cutoff.
    // Player craft must remain available to the other players and their HUDs.
    bAlwaysRelevant = true;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(ShipMesh);
    CameraBoom->TargetArmLength = 200.0f;
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = true;
    CameraBoom->bInheritYaw = true;
    CameraBoom->bInheritRoll = true;
    CameraBoom->SetUsingAbsoluteRotation(false);
    CameraBoom->bEnableCameraLag = false;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 15.0f;
    CameraBoom->bClampToMaxPhysicsDeltaTime = true;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    ThirdPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
    ThirdPersonCamera->bUsePawnControlRotation = false;
    ThirdPersonCamera->SetActive(true);

    CockpitCameraMount = CreateDefaultSubobject<USceneComponent>(TEXT("CockpitCameraMount"));
    CockpitCameraMount->SetupAttachment(ShipMesh);
    CockpitCameraMount->SetActive(false);
    CockpitCameraMount->SetUsingAbsoluteRotation(false);
    CockpitCameraMount->SetRelativeLocation(GetFirstPersonCameraOffset());

    CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
    CockpitCamera->SetupAttachment(CockpitCameraMount);
    CockpitCamera->bUsePawnControlRotation = false;
    CockpitCamera->SetActive(false);

    MissileWarningSynth =
		CreateDefaultSubobject<UAvionicsSynthComponent>(
			TEXT("MissileWarningSynth")
		);

	MissileLockSynth =
		CreateDefaultSubobject<UAvionicsSynthComponent>(
			TEXT("MissileLockSynth")
		);

    MissileVoiceWarningComponent =
        CreateDefaultSubobject<UAudioVoiceWarningComponent>(
            TEXT("MissileVoiceWarningComponent")
        );
}

void APlayerShip::BeginPlay()
{
    // Apply after legacy Blueprint defaults so existing player assets inherit
    // the multiplayer policy without requiring an asset resave.
    bAlwaysRelevant = true;
    bOnlyRelevantToOwner = false;
    bNetUseOwnerRelevancy = false;
    Super::BeginPlay();
    // Sample the corrected physics pose once physics has finished this frame.
    CameraBoom->PrimaryComponentTick.TickGroup = TG_PostPhysics;
    CameraBoom->bUseCameraLagSubstepping = true;
    CameraBoom->AddTickPrerequisiteActor(this);
    SetCameraMode(InitialCameraMode);

    if (LaserWeaponComponent)
    {
        LaserWeaponComponent->OnLaserFired.AddDynamic(
            this,
            &APlayerShip::HandleLaserFired
        );
    }
}

void APlayerShip::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateCatapultCameraShake();
    if(IsLocallyControlled())
        if(const auto* Settings=USpaceAceUserSettings::Get())
        {
            if(ThirdPersonCamera)ThirdPersonCamera->SetFieldOfView(Settings->FieldOfView);
            if(CockpitCamera)CockpitCamera->SetFieldOfView(Settings->FieldOfView);
        }

    if (!IsLocallyControlled() || IsDead() || !IsCombatEnabled() || IsInPreMatchFlight())
    {
        StopAllAvionicsAudio();
        return;
    }

    UpdateMissileWarningAudio(DeltaTime);
	UpdateMissileLockAudio();
}

void APlayerShip::UnPossessed()
{
    StopFlightCameraShakes();
    StopAllAvionicsAudio();
    Super::UnPossessed();
}

void APlayerShip::EndPlay(const EEndPlayReason:: Type EndPlayReason)
{
    StopFlightCameraShakes();
    StopAllAvionicsAudio();

    Super::EndPlay(EndPlayReason);
}

void APlayerShip::HandleLookInput(
	const FVector2D& LookValue
)
{
	float DesiredYaw = 0.0f;

	if (LookValue.X >= LookInputThreshold)
	{
		DesiredYaw = SideLookYaw;
	}
	else if (LookValue.X <= -LookInputThreshold)
	{
		DesiredYaw = -SideLookYaw;
	}
	else if (LookValue.Y <= -LookInputThreshold)
	{
		DesiredYaw = RearLookYaw;
	}

	const FRotator DesiredRotation(
		0.0f,
		DesiredYaw,
		0.0f
	);

	switch (CameraMode)
	{
	case ECameraMode::ThirdPerson:
		if (CameraBoom)
		{
			CameraBoom->SetRelativeRotation(
				DesiredRotation
			);
		}
		break;

	case ECameraMode::Cockpit:
		if (CockpitCameraMount)
		{
			CockpitCameraMount->SetRelativeRotation(
				DesiredRotation
			);
		}
		break;
	}
}

void APlayerShip::ResetLookInput()
{
    LookYaw = 0.0f;
    LookPitch = 0.0f;

    if (CameraBoom)
    {
        CameraBoom->SetRelativeRotation(FRotator::ZeroRotator);
    }

    if (CockpitCameraMount)
    {
        CockpitCameraMount->SetRelativeRotation(FRotator::ZeroRotator);
    }
}

void APlayerShip::UpdateMissileWarningAudio(
    float DeltaTime
)
{
    // Always stop here when no missile is incoming.
    if (!HasIncomingMissile())
    {
        if (bMissileWarningAudioActive)
        {
            StopMissileWarningAudio();
            StopMissileVoiceWarning();
        }

        MissileWarningTimeRemaining = 0.0f;
        return;
    }

    // We now know that a missile is incoming.
    if (!bMissileWarningAudioActive)
    {
        // Stop the lock tone only once, when the warning begins.
        if (bMissileLockAudioActive)
        {
            StopMissileLockAudio();
        }

        bMissileWarningAudioActive = true;
        MissileWarningTimeRemaining = 0.0f;
    }

    PlayMissileVoiceWarning();

    const float MissileDistance =
        GetClosestIncomingMissileDistance();

    const float CurrentWarningInterval =
        FMath::GetMappedRangeValueClamped(
            FVector2D(
                MissileWarningNearDistance,
                MissileWarningFarDistance
            ),
            FVector2D(
                MinimumMissileWarningInterval,
                MaximumMissileWarningInterval
            ),
            MissileDistance
        );

    MissileWarningTimeRemaining -= DeltaTime;

    if (MissileWarningTimeRemaining <= 0.0f)
    {
        PlayMissileWarningBeep();

        MissileWarningTimeRemaining =
            CurrentWarningInterval;
    }
}

void APlayerShip::PlayMissileWarningBeep()
{
	if (
		!HasIncomingMissile() ||
		!MissileWarningSynth
	)
	{
		return;
	}

	MissileWarningSynth->PlayBeep(
		MissileWarningFrequency,
		0.08f,
		0.25f
	);
}

void APlayerShip::StopMissileWarningAudio()
{
	bMissileWarningAudioActive = false;

	if (MissileWarningSynth)
	{
		MissileWarningSynth->Stop();
	}
}

void APlayerShip::PlayMissileVoiceWarning()
{
	if (!MissileVoiceWarningComponent || !HasIncomingMissile())
	{
		return;
	}

	if (!bMissileVoiceWarningActive)
	{
		MissileVoiceWarningComponent->PlayMissileVoiceWarning();
		bMissileVoiceWarningActive = true;
		MissileVoiceWarningTimeRemaining = MissileVoiceWarningInterval;
		return;
	}

	// Decrement timer and replay when expired
	MissileVoiceWarningTimeRemaining -= GetWorld()->GetDeltaSeconds();

	if (MissileVoiceWarningTimeRemaining <= 0.0f)
	{
		MissileVoiceWarningComponent->PlayMissileVoiceWarning();
		MissileVoiceWarningTimeRemaining = MissileVoiceWarningInterval;
	}
}

void APlayerShip::StopMissileVoiceWarning()
{
    if (!MissileVoiceWarningComponent)
    {
        return;
    }

    if (bMissileVoiceWarningActive)
    {
        MissileVoiceWarningComponent->StopMissileVoiceWarning();
        bMissileVoiceWarningActive = false;
    }
}

void APlayerShip::UpdateMissileLockAudio()
{
    if (!IsLocallyControlled() || IsDead() || !IsCombatEnabled() || IsInPreMatchFlight() ||
        HasIncomingMissile() || !IsCurrentSecondaryWeaponHoming() || !TargetingComponent)
    {
        StopMissileLockAudio();
        return;
    }

    const float Progress = TargetingComponent->GetLockOnProgress();
    AActor* Target = TargetingComponent->GetCurrentTarget();
    if (AudioLockTarget.Get() != Target || Progress < PreviousLockProgress)
    {
        StopMissileLockAudio();
        AudioLockTarget = Target;
    }
    PreviousLockProgress = Progress;
    const bool bLocked = TargetingComponent->IsLockedOn();
    const float DeltaTime = GetWorld()->GetDeltaSeconds();
    if (bLocked)
    {
        if (!bMissileLockAudioActive)
        {
            // A high confirmation chirp resolves into the existing steady lock tone.
            if (MissileLockSynth) MissileLockSynth->PlayBeep(MissileLockFrequency * 1.5f, 0.12f, 0.25f);
            bMissileLockAudioActive = true;
            MissileLockToneTimerRemaining = 0.14f;
        }
        else
        {
            MissileLockToneTimerRemaining -= DeltaTime;
            if (MissileLockToneTimerRemaining <= 0.0f)
            {
                PlayMissileLockAudio();
                MissileLockToneTimerRemaining = 4.8f;
            }
        }
    }
    else if (Progress > 0.0f)
    {
        AcquisitionBeepTimer -= DeltaTime;
        if (AcquisitionBeepTimer <= 0.0f)
        {
            if (MissileLockSynth) MissileLockSynth->PlayBeep(MissileLockFrequency, 0.045f, 0.18f);
            AcquisitionBeepTimer = FMath::Lerp(0.22f, 0.065f, Progress);
        }
    }
    else
    {
        StopMissileLockAudio();
    }
}

void APlayerShip::PlayMissileLockAudio()
{
	if (!MissileLockSynth)
	{
		return;
	}

	MissileLockSynth->PlayTone(
		MissileLockFrequency,
		5.0f,
		0.25f
	);
}

void APlayerShip::StopMissileLockAudio()
{
	bMissileLockAudioActive = false;
    AcquisitionBeepTimer = 0.0f;
    PreviousLockProgress = 0.0f;
    MissileLockToneTimerRemaining = 0.0f;
    AudioLockTarget.Reset();

	if (MissileLockSynth)
	{
		MissileLockSynth->Stop();
	}
}

void APlayerShip::SetCameraMode(ECameraMode NewCameraMode)
{
    CameraMode = NewCameraMode;
    const bool bUseThirdPerson = CameraMode == ECameraMode::ThirdPerson;
    if (ThirdPersonCamera) ThirdPersonCamera->SetActive(bUseThirdPerson);
    if (CockpitCamera) CockpitCamera->SetActive(!bUseThirdPerson);
    ResetLookInput();
}

void APlayerShip::StopAllAvionicsAudio()
{
    StopMissileLockAudio();
    StopMissileWarningAudio();

    if (MissileWarningSynth)
    {
        MissileWarningSynth->Stop();
    }
    if (MissileLockSynth)
    {
        MissileLockSynth->Stop();
    }
}

void APlayerShip::SwitchCameraMode()
{
    if (!ThirdPersonCamera || !CockpitCamera)
    {
        return;
    }

    const bool bSwitchingToCockpit =
        CameraMode == ECameraMode::ThirdPerson;

    CameraMode = bSwitchingToCockpit
        ? ECameraMode::Cockpit
        : ECameraMode::ThirdPerson;

    ThirdPersonCamera->SetActive(!bSwitchingToCockpit);
    CockpitCamera->SetActive(bSwitchingToCockpit);

    ResetLookInput();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Camera switched to: %s"),
        bSwitchingToCockpit
            ? TEXT("Cockpit")
            : TEXT("Third Person")
    );
}

float APlayerShip::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    const bool bMissile = IsValid(DamageCauser) && DamageCauser->IsA<AMissileProjectile>();
    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (HasAuthority() && Applied > 0.f && !IsDead()) ClientFlightShake(bMissile ? 2 : 1);
    return Applied;
}

void APlayerShip::HandleLaserFired()
{
    if (!HasAuthority() || !GetWorld()) return;
    // The weapon broadcasts once per muzzle. One recoil per volley is sufficient.
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastLaserShakeTime < .04) return;
    LastLaserShakeTime = Now;
    ClientFlightShake(0);
}

void APlayerShip::ClientFlightShake_Implementation(uint8 Event)
{
    if (IsDead()) return;
    if (Event == 0)
        PlayCameraShake(LaserFireCameraShakeClass ? LaserFireCameraShakeClass.Get() : ULaserFireCameraShake::StaticClass(),
            LaserFireCameraShakeClass ? LaserFireShakeScale : 1.f);
    else if (Event == 1) PlayCameraShake(ULaserHitCameraShake::StaticClass(), 1.f);
    else if (Event == 2) PlayCameraShake(UMissileHitCameraShake::StaticClass(), 1.f);
}

void APlayerShip::UpdateCatapultCameraShake()
{
    const auto* Settings = USpaceAceUserSettings::Get();
    if (!IsLocallyControlled() || IsDead() || (Settings && !Settings->bCameraShake))
    {
        StopFlightCameraShakes();
        return;
    }
    if (!bOnCatapult || !bCatapultLaunching)
    {
        bCatapultShakePlayed = false;
        if (auto* Manager = ShakeCameraManager.Get())
            Manager->StopAllInstancesOfCameraShake(UCatapultCameraShake::StaticClass(), false);
        return;
    }
    const auto* State = GetWorld()->GetGameState();
    const double Now = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    if (!bCatapultShakePlayed && Now >= CatapultStartedAt)
    {
        bCatapultShakePlayed = true;
        PlayCameraShake(UCatapultCameraShake::StaticClass(), 1.f);
    }
}

void APlayerShip::StopFlightCameraShakes()
{
    if (auto* Manager = ShakeCameraManager.Get())
    {
        Manager->StopAllInstancesOfCameraShake(UCatapultCameraShake::StaticClass(), true);
        Manager->StopAllInstancesOfCameraShake(UMissileHitCameraShake::StaticClass(), true);
        Manager->StopAllInstancesOfCameraShake(ULaserHitCameraShake::StaticClass(), true);
        Manager->StopAllInstancesOfCameraShake(ULaserFireCameraShake::StaticClass(), true);
        if (LaserFireCameraShakeClass) Manager->StopAllInstancesOfCameraShake(LaserFireCameraShakeClass, true);
    }
    ShakeCameraManager.Reset();
    bCatapultShakePlayed = false;
}

void APlayerShip::PlayCameraShake(
	TSubclassOf<UCameraShakeBase> CameraShakeClass,
	float ShakeIntensity
)
{
	if (!IsLocallyControlled() || !CameraShakeClass || ShakeIntensity <= 0.0f || (USpaceAceUserSettings::Get()&&!USpaceAceUserSettings::Get()->bCameraShake))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->PlayerCameraManager ||
        PlayerController->GetViewTarget() != this)
	{
		return;
	}

	ShakeCameraManager = PlayerController->PlayerCameraManager;
	PlayerController->PlayerCameraManager->StartCameraShake(
		CameraShakeClass,
		ShakeIntensity
	);
}
