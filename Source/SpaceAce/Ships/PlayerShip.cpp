#include "PlayerShip.h"

#include "HealthComponent.h"
#include "TargetingComponent.h"
#include "LaserWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "AvionicsSynthComponent.h"
#include "AudioVoiceWarningComponent.h"
#include "TimerManager.h"

APlayerShip::APlayerShip()
{
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
    Super::BeginPlay();
    SetCameraMode(InitialCameraMode);

    HealthComponent->OnDamageReceived.AddDynamic(
        this,
        &APlayerShip::HandleDamageFeedback
    );

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

    UpdateMissileWarningAudio(DeltaTime);
	UpdateMissileLockAudio();
}

void APlayerShip::EndPlay(const EEndPlayReason:: Type EndPlayReason)
{
    StopMissileWarningAudio();

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
    if (HasIncomingMissile())
    {
        bMissileLockAudioActive = false;
        MissileLockToneTimerRemaining = 0.0f;
        return;
    }

    const bool bLocked =
        TargetingComponent &&
        TargetingComponent->IsLockedOn();

    if (bLocked && !bMissileLockAudioActive)
    {
        bMissileLockAudioActive = true;
        MissileLockToneTimerRemaining = 0.0f;
        PlayMissileLockAudio();
    }
    else if (bLocked && bMissileLockAudioActive)
    {
        // Keep the tone looping while locked on
        MissileLockToneTimerRemaining -= GetWorld()->GetDeltaSeconds();

        if (MissileLockToneTimerRemaining <= 0.0f)
        {
            PlayMissileLockAudio();
            MissileLockToneTimerRemaining = 4.8f;
        }
    }
    else if (!bLocked && bMissileLockAudioActive)
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

void APlayerShip::HandleDamageFeedback(float DamageAmount)
{
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	const float NormalizedDamage = FMath::Clamp(
		DamageAmount / DamageForMaximumShake,
		0.0f,
		1.0f
	);

	const float ShakeIntensity = FMath::Lerp(
		MinimumDamageShakeScale,
		1.0f,
		NormalizedDamage
	);

	PlayCameraShake(DamageCameraShakeClass, ShakeIntensity);
}

void APlayerShip::HandleLaserFired()
{
	PlayCameraShake(LaserFireCameraShakeClass, LaserFireShakeScale);
}

void APlayerShip::PlayCameraShake(
	TSubclassOf<UCameraShakeBase> CameraShakeClass,
	float ShakeIntensity
)
{
	if (!CameraShakeClass || ShakeIntensity <= 0.0f)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	PlayerController->PlayerCameraManager->StartCameraShake(
		CameraShakeClass,
		ShakeIntensity
	);
}
