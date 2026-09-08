// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerControllerCPP.h"
#include "ShipBase.h"
#include "PlayerShip.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

APlayerControllerCPP::APlayerControllerCPP()
{
}

void APlayerControllerCPP::BeginPlay()
{
    Super::BeginPlay();

    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);

    bShowMouseCursor = false;
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (IMC_Player)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("Using IMC: %s | Path: %s | Mapping count: %d"),
                    *IMC_Player->GetName(),
                    *IMC_Player->GetPathName(),
                    IMC_Player->GetMappings().Num()
                );

				Subsystem->RemoveMappingContext(IMC_Player);
                Subsystem->AddMappingContext(IMC_Player, 0);
            }
        }
    }
}

void APlayerControllerCPP::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(InputComponent);

    if (!EnhancedInputComponent)
    {
        return;
    }

    if (IA_Throttle)
    {
        EnhancedInputComponent->BindAction(
            IA_Throttle,
            ETriggerEvent::Triggered,
            this,
            &APlayerControllerCPP::HandleThrottle
        );

        EnhancedInputComponent->BindAction(
            IA_Throttle,
            ETriggerEvent::Completed,
            this,
            &APlayerControllerCPP::HandleThrottle
        );
    }

    if (IA_Pitch)
    {
        EnhancedInputComponent->BindAction(
            IA_Pitch,
            ETriggerEvent::Triggered,
            this,
            &APlayerControllerCPP::HandlePitch
        );

        EnhancedInputComponent->BindAction(
            IA_Pitch,
            ETriggerEvent::Completed,
            this,
            &APlayerControllerCPP::HandlePitch
        );
    }

    if (IA_Yaw)
    {
        EnhancedInputComponent->BindAction(
            IA_Yaw,
            ETriggerEvent::Triggered,
            this,
            &APlayerControllerCPP::HandleYaw
        );

        EnhancedInputComponent->BindAction(
            IA_Yaw,
            ETriggerEvent::Completed,
            this,
            &APlayerControllerCPP::HandleYaw
        );
    }

    if (IA_Roll)
    {
        EnhancedInputComponent->BindAction(
            IA_Roll,
            ETriggerEvent::Triggered,
            this,
            &APlayerControllerCPP::HandleRoll
        );

        EnhancedInputComponent->BindAction(
            IA_Roll,
            ETriggerEvent::Completed,
            this,
            &APlayerControllerCPP::HandleRoll
        );
    }

    if (IA_Gun)
    {
        EnhancedInputComponent->BindAction(
            IA_Gun,
            ETriggerEvent::Started,
            this,
            &APlayerControllerCPP::HandleGun
        );

        EnhancedInputComponent->BindAction(
            IA_Gun,
            ETriggerEvent::Completed,
            this,
            &APlayerControllerCPP::HandleGun
        );
    }

    if (IA_Missile)
    {
        EnhancedInputComponent->BindAction(
            IA_Missile,
            ETriggerEvent::Started,
            this,
            &APlayerControllerCPP::HandleMissile
        );
    }

    if (IA_Look)
    {
        EnhancedInputComponent->BindAction(
            IA_Look,
            ETriggerEvent::Triggered,
            this,
            &APlayerControllerCPP::HandleLook
        );

        EnhancedInputComponent->BindAction(
            IA_Look,
            ETriggerEvent::Completed,
            this,
            &APlayerControllerCPP::HandleLook
        );
    }

    if (IA_SwitchTarget)
    {
        EnhancedInputComponent->BindAction(
            IA_SwitchTarget,
            ETriggerEvent::Started,
            this,
            &APlayerControllerCPP::HandleSwitchTarget
        );
    }

	if (IA_SwitchCamera)
	{
		EnhancedInputComponent->BindAction(
			IA_SwitchCamera,
			ETriggerEvent::Started,
			this,
			&APlayerControllerCPP::HandleSwitchCamera
		);
	}

	if (IA_SwitchSecondaryWeapon)
	{
		EnhancedInputComponent->BindAction(
			IA_SwitchSecondaryWeapon,
			ETriggerEvent::Started,
			this,
			&APlayerControllerCPP::HandleSwitchSecondaryWeapon
		);
	}

	if (IA_Pause)
	{
		EnhancedInputComponent->BindAction(
			IA_Pause,
			ETriggerEvent::Started,
			this,
			&APlayerControllerCPP::HandlePause
		);
	}
}

void APlayerControllerCPP::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ControlledShip = Cast<AShipBase>(InPawn);

    ControlledPlayerShip = Cast<APlayerShip>(InPawn);
}

void APlayerControllerCPP::OnUnPossess()
{
    ControlledPlayerShip = nullptr;
    
    ControlledShip = nullptr;

    Super::OnUnPossess();
}

void APlayerControllerCPP::HandleThrottle(const FInputActionValue& Value)
{
    const float ThrottleValue = Value.Get<float>();

    if (ControlledShip)
    {
        ControlledShip->SetThrottleInput(ThrottleValue);
        /* UE_LOG(
            LogTemp,
            Warning,
            TEXT("Throttle Value: %f"),
            ThrottleValue
        ); */
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("No Controlled Ship to set Throttle Input.")
        );
    }
}

void APlayerControllerCPP::HandlePitch(const FInputActionValue& Value)
{
    const float PitchValue = Value.Get<float>();

    if (ControlledShip)
    {
        ControlledShip->SetPitchInput(PitchValue);
    }

    /* UE_LOG(
        LogTemp,
        Warning,
        TEXT("Pitch Value: %f"),
        PitchValue
    ); */
}

void APlayerControllerCPP::HandleYaw(const FInputActionValue& Value)
{
    const float YawValue = Value.Get<float>();

    if (ControlledShip)
    {
        ControlledShip->SetYawInput(YawValue);
    }

/*   UE_LOG(
        LogTemp,
        Warning,
        TEXT("Yaw Value: %f"),
        YawValue
    ); */
}

void APlayerControllerCPP::HandleRoll(const FInputActionValue& Value)
{
    const float RollValue = Value.Get<float>();

    if (ControlledShip)
    {
        ControlledShip->SetRollInput(RollValue);
    }

    /* UE_LOG(
        LogTemp,
        Warning,
        TEXT("Roll Value: %f"),
        RollValue
    ); */
}

void APlayerControllerCPP::HandleGun(const FInputActionValue& Value)
{
    const bool bIsFiring = Value.Get<bool>();

    if (ControlledShip)
    {
        if (bIsFiring)
        {
            ControlledShip->StartFiringLasers();
        }
        else
        {
            ControlledShip->StopFiringLasers();
        }
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Gun Firing: %s"),
        bIsFiring ? TEXT("True") : TEXT("False")
    );
}

void APlayerControllerCPP::HandleMissile(const FInputActionValue& Value)
{
    const bool bIsFiring = Value.Get<bool>();

    if (ControlledShip && bIsFiring)
    {
        ControlledShip->FireSingleMissile();
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Missile Firing: %s"),
        bIsFiring ? TEXT("True") : TEXT("False")
    );
}

void APlayerControllerCPP::HandleLook(const FInputActionValue& Value)
{
    const FVector2D LookValue = Value.Get<FVector2D>();

    if (!ControlledPlayerShip)
    {
        return;
    }

    ControlledPlayerShip->HandleLookInput(Value.Get<FVector2D>());
}

void APlayerControllerCPP::HandleSwitchTarget(const FInputActionValue& Value)
{
    const bool bIsSwitching = Value.Get<bool>();

    if (ControlledShip && bIsSwitching)
    {
        ControlledShip->SwitchTarget();
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Switch Target: %s"),
        bIsSwitching ? TEXT("True") : TEXT("False")
    );
}

void APlayerControllerCPP::HandleSwitchCamera(
	const FInputActionValue& Value
)
{
	if (ControlledPlayerShip && Value.Get<bool>())
	{
		ControlledPlayerShip->SwitchCameraMode();
	}
}

void APlayerControllerCPP::HandleSwitchSecondaryWeapon(
	const FInputActionValue& Value
)
{
	if (ControlledShip && Value.Get<bool>())
	{
		ControlledShip->SwitchSecondaryWeaponMode();
	}
}

void APlayerControllerCPP::HandlePause(const FInputActionValue& Value)
{
	if (Value.Get<bool>())
	{
		TogglePauseMenu();
	}
}

void APlayerControllerCPP::TogglePauseMenu()
{
	if (IsPauseMenuOpen())
	{
		ResumeGame();
	}
	else
	{
		OpenPauseMenu();
	}
}

void APlayerControllerCPP::OpenPauseMenu()
{
	if (IsPauseMenuOpen() || !PauseMenuClass)
	{
		if (!PauseMenuClass)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("PlayerControllerCPP: PauseMenuClass is not assigned.")
			);
		}
		return;
	}

	ActivePauseMenu = CreateWidget<UUserWidget>(this, PauseMenuClass);
	if (!ActivePauseMenu)
	{
		return;
	}

	if (ControlledShip)
	{
		ControlledShip->StopFiringLasers();
	}

	ActivePauseMenu->AddToViewport(100);
	SetPause(true);

    if (PauseSoundMix)
    {
        UGameplayStatics::PushSoundMixModifier(this, PauseSoundMix);
    }

	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ActivePauseMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void APlayerControllerCPP::ResumeGame()
{
	if (ActivePauseMenu)
	{
		ActivePauseMenu->RemoveFromParent();
		ActivePauseMenu = nullptr;
	}

	SetPause(false);
	bShowMouseCursor = false;

    if (PauseSoundMix)
    {
        UGameplayStatics::PopSoundMixModifier(this, PauseSoundMix);
    }

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

bool APlayerControllerCPP::IsPauseMenuOpen() const
{
	return IsValid(ActivePauseMenu) && ActivePauseMenu->IsInViewport();
}
