#include "ShipSensingComponent.h"
#include "CombatantSubsystem.h"
#include "GameplayTagAssetInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Sets default values for this component's properties
UShipSensingComponent::UShipSensingComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Disable ticking for this component
}


// Called when the game starts
void UShipSensingComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float InitialDelay = FMath::FRandRange(
		0.0f,
		SenseUpdateInterval
	);

	World->GetTimerManager().SetTimer(
		SenseUpdateTimerHandle,
		this,
		&UShipSensingComponent::UpdateSenses,
		SenseUpdateInterval,
		true,
		InitialDelay
	);
	
}

void UShipSensingComponent::Configure(const FGameplayTag& NewEnemyTeamTag)
{
	EnemyTeamTag = NewEnemyTeamTag;
}

const TArray<FPerceivedContact>&
UShipSensingComponent::GetPerceivedContacts() const
{
	return PerceivedContacts;
}
void UShipSensingComponent::UpdateSenses()
{
	for (FPerceivedContact& Contact : PerceivedContacts)
	{
		Contact.bCurrentlyVisible = false;
	}
	
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!IsValid(Owner) || !World)
	{
		return;
	}

	UCombatantSubsystem* CombatantSubsystem = 
		World->GetSubsystem<UCombatantSubsystem>();
	
	if (!CombatantSubsystem)
	{
		return;
	}

	TArray<AActor*> NearbyCombatants;

	CombatantSubsystem->GetCombatantsInRange(
		Owner->GetActorLocation(),
		SensorRange,
		NearbyCombatants
	);

	for (AActor* Candidate : NearbyCombatants)
	{
		if (!IsValid(Candidate) || Candidate == Owner)
		{
			continue;
		}

		if (!IsEnemyCombatant(Candidate))
		{
			continue;
		}

		if (!IsInsideFieldOfView(Candidate))
		{
			continue;
		}

		UpdateOrCreateContact(Candidate);
	}

	ForgetExpiredContacts();

	int32 VisibleContactCount = 0;

	for (const FPerceivedContact& Contact : PerceivedContacts)
	{
		if (Contact.bCurrentlyVisible)
		{
			++VisibleContactCount;
		}
	}
}

bool UShipSensingComponent::IsEnemyCombatant(AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	IGameplayTagAssetInterface* TagInterface =
		Cast<IGameplayTagAssetInterface>(Candidate);
	
	if (!TagInterface)
	{
		return false;
	}

	FGameplayTagContainer CandidateTags;
	TagInterface->GetOwnedGameplayTags(CandidateTags);

	return CandidateTags.HasTag(EnemyTeamTag);
}

bool UShipSensingComponent::IsInsideFieldOfView(AActor* Candidate) const
{
	const AActor* Owner = GetOwner();

	if (!Owner || !IsValid(Candidate))
	{
		return false;
	}

	const FVector DirectionToCandidate =
		(Candidate->GetActorLocation() - 
		Owner->GetActorLocation()
	).GetSafeNormal();

	const float SightAlignment = FVector::DotProduct(
		Owner->GetActorForwardVector(),
		DirectionToCandidate
	);

	return SightAlignment >= MinimumSightDotProduct;
}

FPerceivedContact* UShipSensingComponent::FindContact(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	for (FPerceivedContact& Contact : PerceivedContacts)
	{
		if (Contact.Actor == Actor)
		{
			return &Contact;
		}
	}

	return nullptr;
}

void UShipSensingComponent::UpdateOrCreateContact(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	FPerceivedContact* ExistingContact = FindContact(Actor);

	if (ExistingContact)
	{
		ExistingContact->LastKnownLocation = Actor->GetActorLocation();
		ExistingContact->LastKnownVelocity = Actor->GetVelocity();
		ExistingContact->LastSeenTime = GetWorld()->GetTimeSeconds();
		ExistingContact->bCurrentlyVisible = true;

		return;
	}

	FPerceivedContact NewContact;
	NewContact.Actor = Actor;
	NewContact.LastKnownLocation = Actor->GetActorLocation();
	NewContact.LastKnownVelocity = Actor->GetVelocity();
	NewContact.LastSeenTime = GetWorld()->GetTimeSeconds();
	NewContact.bCurrentlyVisible = true;

	PerceivedContacts.Add(NewContact);
}

void UShipSensingComponent::ForgetExpiredContacts()
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	for (int32 Index = PerceivedContacts.Num() - 1; Index >= 0; --Index)
	{
		const FPerceivedContact& Contact = PerceivedContacts[Index];

		const bool bActorInvalid = !Contact.Actor.IsValid();

		const bool bMemoryExpired = 
			CurrentTime - Contact.LastSeenTime > ContactMemoryDuration;

		if (bActorInvalid || bMemoryExpired)
		{
			PerceivedContacts.RemoveAt(Index);
		}
	}
	
}

int32 UShipSensingComponent::GetRememberedContactCount() const
{
	return PerceivedContacts.Num();
}

int32 UShipSensingComponent::GetVisibleContactCount() const
{
	int32 VisibleCount = 0;

	for (const FPerceivedContact& Contact : PerceivedContacts)
	{
		if (Contact.bCurrentlyVisible)
		{
			++VisibleCount;
		}
	}

	return VisibleCount;
}