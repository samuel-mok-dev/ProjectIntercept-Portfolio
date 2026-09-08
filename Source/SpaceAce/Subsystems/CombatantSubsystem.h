// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatantSubsystem.generated.h"

class AActor;

/**
 * Maintains a registry of all combatant actors in the world. 
 * Sensors can query this subsystem instead of scanning every actor in the
 * world.
 **/
UCLASS()
class SPACEACE_API UCombatantSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	void RegisterCombatant(AActor* Combatant);
	void UnregisterCombatant(AActor* Combatant);

	void GetCombatantsInRange(
		const FVector& Origin,
		float Radius,
		TArray<AActor*>& OutCombatants
	);

	int32 GetRegisteredCombatantCount() const;

private:
	void RemoveInvalidCombatants();

	/*
	* Weak pointers allow actors to disappear without the subsystem
	* keeping them alive
	*/
	TSet<TWeakObjectPtr<AActor>> RegisteredCombatants;
};
