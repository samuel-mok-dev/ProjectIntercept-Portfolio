// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerControllerCPP.h"
#include "Net/UnrealNetwork.h"
#include "SpaceAceUserSettings.h"
#include "SpaceAceSettingsWidget.h"
#include "ShipBase.h"
#include "PlayerShip.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "CountermeasureComponent.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "PrivatePlaytestGameMode.h"

void APlayerControllerCPP::ServerSelectPlaytestFighter_Implementation(int32 TeamID, FName FighterID)
{
    if (auto* Mode = GetWorld()->GetAuthGameMode<APrivatePlaytestGameMode>()) Mode->SelectFighter(this, TeamID, FighterID);
}

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
                ApplyInputBindings();
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
        EnhancedInputComponent->BindAction(IA_Throttle, ETriggerEvent::Canceled, this, &APlayerControllerCPP::HandleThrottle);
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
        EnhancedInputComponent->BindAction(IA_Pitch, ETriggerEvent::Canceled, this, &APlayerControllerCPP::HandlePitch);
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
        EnhancedInputComponent->BindAction(IA_Yaw, ETriggerEvent::Canceled, this, &APlayerControllerCPP::HandleYaw);
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
        EnhancedInputComponent->BindAction(IA_Roll, ETriggerEvent::Canceled, this, &APlayerControllerCPP::HandleRoll);
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
        EnhancedInputComponent->BindAction(IA_Gun, ETriggerEvent::Canceled, this, &APlayerControllerCPP::HandleGun);
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
        EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Canceled, this, &APlayerControllerCPP::HandleLook);
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

    if (!IA_Countermeasures) IA_Countermeasures=LoadObject<UInputAction>(nullptr,TEXT("/Game/Inputs/IA_Countermeasures.IA_Countermeasures"));
    if (IA_Countermeasures) EnhancedInputComponent->BindAction(IA_Countermeasures,ETriggerEvent::Started,this,&APlayerControllerCPP::HandleCountermeasures);
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

void APlayerControllerCPP::RefreshControlledShip()
{
    AShipBase* NewShip = Cast<AShipBase>(GetPawn());
    if (ControlledShip != NewShip)
    {
        CurrentFlightInput = FFlightInput{};
    }
    ControlledShip = NewShip;
    ControlledPlayerShip = Cast<APlayerShip>(GetPawn());
}

void APlayerControllerCPP::SetPawn(APawn* InPawn)
{
    Super::SetPawn(InPawn);
    RefreshControlledShip();
}

void APlayerControllerCPP::OnRep_Pawn()
{
    Super::OnRep_Pawn();
    RefreshControlledShip();
}

void APlayerControllerCPP::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    // Input events have all been processed by Super. Send one coherent state,
    // including zeroes after release, instead of up to four RPCs per frame.
    if (IsLocalController() && ControlledShip)
    {
        if (IsPauseMenuOpen()) CurrentFlightInput = FFlightInput{};
        if (HasAuthority())
        {
            ControlledShip->SetFlightInput(CurrentFlightInput);
        }
        else
        {
            FlightInputSendElapsed += DeltaTime;
            if (FlightInputSendElapsed >= 1.0f / 30.0f)
            {
                FlightInputSendElapsed = FMath::Fmod(FlightInputSendElapsed, 1.0f / 30.0f);
                ServerSetFlightInput(CurrentFlightInput);
            }
        }
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
    if (ControlledShip && HasAuthority())
    {
        ControlledShip->StopFiringLasers();
        ControlledShip->SetFlightInput(FFlightInput{});
    }
    CurrentFlightInput = FFlightInput{};
    ControlledPlayerShip = nullptr;
    
    ControlledShip = nullptr;

    Super::OnUnPossess();
}

void APlayerControllerCPP::HandleThrottle(const FInputActionValue& Value)
{
    const float ThrottleValue = Value.Get<float>();

    if (ControlledShip)
    {
        CurrentFlightInput.Throttle = ThrottleValue;

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
    const auto* Settings=USpaceAceUserSettings::Get();
    const float PitchValue = Settings ? FMath::Clamp(Settings->FilterAxis(Value.Get<float>())*Settings->FlightSensitivity*(Settings->bInvertPitch?-1.f:1.f),-1.f,1.f) : Value.Get<float>();

    if (ControlledShip)
    {
        CurrentFlightInput.Pitch = PitchValue;

    }
}

void APlayerControllerCPP::HandleYaw(const FInputActionValue& Value)
{
    const auto* Settings=USpaceAceUserSettings::Get();
    const float YawValue = Settings ? FMath::Clamp(Settings->FilterAxis(Value.Get<float>())*Settings->FlightSensitivity,-1.f,1.f) : Value.Get<float>();

    if (ControlledShip)
    {
        CurrentFlightInput.Yaw = YawValue;

    }
}

void APlayerControllerCPP::HandleRoll(const FInputActionValue& Value)
{
    const auto* Settings=USpaceAceUserSettings::Get();
    const float RollValue = Settings ? FMath::Clamp(Settings->FilterAxis(Value.Get<float>())*Settings->FlightSensitivity,-1.f,1.f) : Value.Get<float>();

    if (ControlledShip)
    {
        CurrentFlightInput.Roll = RollValue;

    }
}

void APlayerControllerCPP::HandleGun(const FInputActionValue& Value)
{
    if (IsPauseMenuOpen()) return;
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
    if (IsPauseMenuOpen()) return;
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

    if(IsPauseMenuOpen())return;
    FVector2D Adjusted=Value.Get<FVector2D>();
    if(const auto* Settings=USpaceAceUserSettings::Get()){Adjusted*=Settings->LookSensitivity;if(Settings->bInvertLook)Adjusted.Y*=-1;}
    ControlledPlayerShip->HandleLookInput(Adjusted);
}

void APlayerControllerCPP::HandleSwitchTarget(const FInputActionValue& Value)
{
    if (IsPauseMenuOpen()) return;
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
    if (IsPauseMenuOpen()) return;
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

void APlayerControllerCPP::HandleCountermeasures(const FInputActionValue& Value)
{
    if (!IsPauseMenuOpen() && ControlledShip && Value.Get<bool>())
        if(auto* Decoys=ControlledShip->FindComponentByClass<UCountermeasureComponent>()) Decoys->Deploy();
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
	if (GetNetMode() == NM_Standalone) SetPause(true);

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
	return (IsValid(ActivePauseMenu) && ActivePauseMenu->IsInViewport()) || USpaceAceSettingsWidget::IsOpen(this);
}

void APlayerControllerCPP::ClearPauseState()
{
    SetPause(false);

    if (PauseSoundMix)
    {
        UGameplayStatics::PopSoundMixModifier(this, PauseSoundMix);
    }
}

void APlayerControllerCPP::ServerSetFlightInput_Implementation(const FFlightInput& NewFlightInput)
{
	if (AShipBase* Ship = Cast<AShipBase>(GetPawn()))
	{
		Ship->SetFlightInput(NewFlightInput);
	}
}

void APlayerControllerCPP::ApplyInputBindings()
{
    if(!GetLocalPlayer()||!IMC_Player)return;
    auto* InputSubsystem=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if(!InputSubsystem)return;
    InputSubsystem->RemoveMappingContext(IMC_Player);
    if(RuntimeInputContext)InputSubsystem->RemoveMappingContext(RuntimeInputContext);
    RuntimeInputContext=DuplicateObject<UInputMappingContext>(IMC_Player,this);
    if(const auto* Settings=USpaceAceUserSettings::Get())
        for(int32 Index=0;Index<RuntimeInputContext->GetMappings().Num();++Index)
        {
            auto& Mapping=RuntimeInputContext->GetMapping(Index);
            if(!Mapping.Action)continue;
            if(const FKey* Override=Settings->KeyOverrides.Find(USpaceAceUserSettings::MappingID(Mapping.Action->GetFName(),Mapping.Key));Override&&Override->IsValid()&&!Override->IsAnalog())Mapping.Key=*Override;
        }
    InputSubsystem->AddMappingContext(RuntimeInputContext,0);
}

void APlayerControllerCPP::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(APlayerControllerCPP,RespawnShipsAhead,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(APlayerControllerCPP,RespawnQueueSize,COND_OwnerOnly);
}
