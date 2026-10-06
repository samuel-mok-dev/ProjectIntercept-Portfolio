// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShipBase.h"
#include "PlayerControllerCPP.generated.h"

class APlayerShip;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class USoundMix;

struct FInputActionValue;

UCLASS()
class SPACEACE_API APlayerControllerCPP : public APlayerController
{
	GENERATED_BODY()

public:
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Respawn") int32 RespawnShipsAhead = INDEX_NONE;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Respawn") int32 RespawnQueueSize = 0;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void ApplyInputBindings();

	APlayerControllerCPP();
    UFUNCTION(Server, Reliable) void ServerSelectPlaytestFighter(int32 TeamID, FName FighterID);
	virtual void SetPawn(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ClearPauseState();

	UFUNCTION(BlueprintPure, Category = "Menu")
	bool IsPauseMenuOpen() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu|Audio")
	TObjectPtr<USoundMix> PauseSoundMix;

protected:
	virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
	
protected:
	// Input mapping context for the player controller
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Player;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RuntimeInputContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Throttle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Pitch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Yaw;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Roll;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Gun;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Missile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_SwitchTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_SwitchCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_SwitchSecondaryWeapon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> IA_Countermeasures;
    void HandleCountermeasures(const FInputActionValue& Value);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Pause;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<UUserWidget> PauseMenuClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActivePauseMenu;

	// Store a reference to the controlled ship
	UPROPERTY(Transient)
	TObjectPtr<AShipBase> ControlledShip;

	UPROPERTY(Transient)
	TObjectPtr<APlayerShip> ControlledPlayerShip;

	void RefreshControlledShip();
	float FlightInputSendElapsed = 0.0f;

	// Current flight input state
	UPROPERTY(Transient)
	FFlightInput CurrentFlightInput;

	// Input handling functions
	void HandleThrottle(const FInputActionValue& Value);
	void HandlePitch(const FInputActionValue& Value);
	void HandleYaw(const FInputActionValue& Value);
	void HandleRoll(const FInputActionValue& Value);
	void HandleGun(const FInputActionValue& Value);
	void HandleMissile(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleSwitchTarget(const FInputActionValue& Value);
	void HandleSwitchCamera(const FInputActionValue& Value);
	void HandleSwitchSecondaryWeapon(const FInputActionValue& Value);
	void HandlePause(const FInputActionValue& Value);

	UFUNCTION(Server, Unreliable)
	void ServerSetFlightInput(const FFlightInput& NewFlightInput);
};
