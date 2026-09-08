#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MissileWeaponComponent.generated.h"

class AActor;
class AMissileProjectile;
class USceneComponent;

UENUM(BlueprintType)
enum class ESecondaryWeaponMode : uint8
{
	StandardMissile UMETA(DisplayName = "Standard Missiles"),
	SpecialWeapon UMETA(DisplayName = "Special Weapon")
};

UCLASS(ClassGroup = (Weapons), meta = (BlueprintSpawnableComponent))
class SPACEACE_API UMissileWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMissileWeaponComponent();

	void Configure(const TArray<USceneComponent*>& NewHardpoints);
	void InitializeMissilePool();

	void FireMissile(AActor* Target, bool bHasLock);
	void SwitchWeaponMode();
	void SetWeaponMode(ESecondaryWeaponMode NewMode);

	bool HasLoadedMissile() const;
	bool HasMissileInTheAir() const;
	float GetEffectiveRange() const;
	int32 GetSpecialWeaponAmmo() const;
	ESecondaryWeaponMode GetWeaponMode() const;
	bool IsCurrentWeaponHoming() const;

	void FinishReload(int32 HardpointIndex);
	void ReturnMissileToPool(AMissileProjectile* Missile);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	int32 MissilePoolSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	float MissileDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	float MissileSpeed = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	float MissileLifetime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	float MissileReloadTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	TSubclassOf<AMissileProjectile> MissileProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	bool bStandardMissileIsHoming = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	bool bStandardMissileInheritsShipVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Standard")
	float StandardMissileGravityScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	TSubclassOf<AMissileProjectile> SpecialProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	int32 SpecialPoolSize = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	int32 SpecialWeaponMaximumAmmo = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	float SpecialWeaponCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	float SpecialWeaponDamage = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	float SpecialWeaponSpeed = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	float SpecialWeaponLifetime = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	bool bSpecialWeaponIsHoming = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	bool bSpecialWeaponInheritsShipVelocity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon|Special")
	float SpecialWeaponGravityScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Weapon")
	TArray<USceneComponent*> MissileHardpoints;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Secondary Weapon")
	ESecondaryWeaponMode WeaponMode = ESecondaryWeaponMode::StandardMissile;

private:
	UPROPERTY()
	TArray<TObjectPtr<AMissileProjectile>> StandardMissilePool;

	UPROPERTY()
	TArray<TObjectPtr<AMissileProjectile>> SpecialWeaponPool;

	TQueue<AMissileProjectile*> AvailableStandardMissiles;
	TQueue<AMissileProjectile*> AvailableSpecialWeapons;
	TSet<TWeakObjectPtr<AMissileProjectile>> SpecialWeaponProjectiles;

	TArray<bool> bHardpointLoaded;
	TArray<FTimerHandle> MissileReloadTimerHandles;

	int32 ActiveProjectileCount = 0;
	int32 RemainingSpecialAmmo = 0;
	int32 NextSpecialHardpoint = 0;
	float LastSpecialFireTime = -1000.0f;
	float EffectiveMissileRange = 20000.0f;

	void BuildPool(
		TSubclassOf<AMissileProjectile> ProjectileClass,
		int32 PoolSize,
		bool bSpecialPool
	);

	AMissileProjectile* GetAvailableProjectile(bool bSpecialPool);
	void FireStandardMissile(AActor* Target, bool bHasLock);
	void FireSpecialWeapon(AActor* Target, bool bHasLock);
	void ReloadMissile(int32 HardpointIndex);
};
