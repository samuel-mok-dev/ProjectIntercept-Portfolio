// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShipBase.h"
#include "PlayerShip.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAvionicsSynthComponent;
class USoundClass;
class UCameraShakeBase;

UENUM(BlueprintType)
enum class ECameraMode : uint8
{
	ThirdPerson UMETA(DisplayName = "Third Person"),
	Cockpit UMETA(DisplayName = "Cockpit")
};

UCLASS()
class SPACEACE_API APlayerShip : public AShipBase
{
	GENERATED_BODY()

public:
	APlayerShip();
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleLookInput(const FVector2D& LookValue);
	void ResetLookInput();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCameraMode(ECameraMode NewCameraMode);

	UFUNCTION(BlueprintPure, Category = "Camera")
	ECameraMode GetCameraMode() const { return CameraMode; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SwitchCameraMode();

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StopAllAvionicsAudio();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> CockpitCameraMount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CockpitCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Look")
	float LookSensitivity = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Look")
	float MinimumLookPitch = -70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Look")
	float MaximumLookPitch = 70.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Look")
	float SideLookYaw = 90.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Look")
	float RearLookYaw = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Look")
	float LookInputThreshold = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UAvionicsSynthComponent> MissileWarningSynth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MissileWarningInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MissileWarningFrequency = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MinimumMissileWarningInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MaximumMissileWarningInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MissileWarningNearDistance = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MissileWarningFarDistance = 20000.0f;

	float MissileWarningTimeRemaining = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UAvionicsSynthComponent> MissileLockSynth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<class UAudioVoiceWarningComponent> MissileVoiceWarningComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MissileLockFrequency = 880.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Shake")
	TSubclassOf<UCameraShakeBase> LaserFireCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Shake")
	float LaserFireShakeScale = 0.2f;

	UFUNCTION()
	void HandleLaserFired();


	void PlayCameraShake(
		TSubclassOf<UCameraShakeBase> CameraShakeClass,
		float ShakeIntensity
	);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Warnings")
	float MissileVoiceWarningInterval = 1.0f;

	float MissileVoiceWarningTimeRemaining = 0.0f;
	float MissileLockToneTimerRemaining = 0.0f;
    float AcquisitionBeepTimer = 0.0f;
    float PreviousLockProgress = 0.0f;
    TWeakObjectPtr<AActor> AudioLockTarget;

	bool bMissileWarningAudioActive = false;
	bool bMissileLockAudioActive = false;
	bool bMissileVoiceWarningActive = false;

	void UpdateMissileWarningAudio(float DeltaTime);
	void PlayMissileWarningBeep();
	void StopMissileWarningAudio();
	void PlayMissileVoiceWarning();
	void StopMissileVoiceWarning();
	void UpdateMissileLockAudio();
	void PlayMissileLockAudio();
	void StopMissileLockAudio();

private:
    // Cosmetic events go only to the owning player; gameplay remains authoritative.
    UFUNCTION(Client, Unreliable)
    void ClientFlightShake(uint8 Event);
    void UpdateCatapultCameraShake();
    void StopFlightCameraShakes();
    double LastLaserShakeTime = -1.;
    bool bCatapultShakePlayed = false;
    TWeakObjectPtr<class APlayerCameraManager> ShakeCameraManager;
	float LookYaw = 0.0f;
	float LookPitch = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	ECameraMode InitialCameraMode = ECameraMode::ThirdPerson;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	ECameraMode CameraMode = ECameraMode::ThirdPerson;
};
