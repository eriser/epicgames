// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"
#include "GameplayPrediction.h"
#include "GameplayAbilityTypes.generated.h"

class UGameplayEffect;
class UAnimInstance;
class UAbilitySystemComponent;
class UGameplayAbility;
class AGameplayAbilityTargetActor;
class UAbilityTask;
class UAttributeSet;

GAMEPLAYABILITIES_API DECLARE_LOG_CATEGORY_EXTERN(LogAbilitySystemComponent, Log, All);

UENUM(BlueprintType)
namespace EGameplayAbilityInstancingPolicy
{
	/**
	 *	How the ability is instanced when executed. This limits what an ability can do in its implementation. For example, a NonInstanced
	 *	Ability cannot have state. It is probably unsafe for an InstancedPerActor ability to have latent actions, etc.
	 */

	enum Type
	{
		NonInstanced,			// This ability is never instanced. Anything that executes the ability is operating on the CDO.
		InstancedPerActor,		// Each actor gets their own instance of this ability. State can be saved, replication is possible.
		InstancedPerExecution,	// We instance this ability each time it is executed. Replication possible but not recommended.
	};
}

UENUM(BlueprintType)
namespace EGameplayAbilityNetExecutionPolicy
{
	/**
	 *	How does an ability execute on the network. Does a client "ask and predict", "ask and wait", "don't ask (just do it)"
	 */

	enum Type
	{
		Predictive		UMETA(DisplayName = "Predictive"),	// Part of this ability runs predictively on the client.
		Server			UMETA(DisplayName = "Server"),		// This ability must be OK'd by the server before doing anything on a client.
		Client			UMETA(DisplayName = "Client"),		// This ability runs as long the client says it does.
	};
}

UENUM(BlueprintType)
namespace EGameplayAbilityReplicationPolicy
{
	/**
	 *	How an ability replicates state/events to everyone on the network.
	 */

	enum Type
	{
		ReplicateNone		UMETA(DisplayName = "Replicate None"),	// We don't replicate the instance of the ability to anyone.
		ReplicateAll		UMETA(DisplayName = "Replicate All"),	// We replicate the instance of the ability to everyone (even simulating clients).
		ReplicateOwner		UMETA(DisplayName = "Replicate Owner"),	// Only replicate the instance of the ability to the owner.
	};
}


UENUM(BlueprintType)
namespace EGameplayAbilityActivationMode
{
	enum Type
	{
		Authority,			// We are the authority activating this ability
		NonAuthority,		// We are not the authority but aren't predicting yet. This is a mostly invalid state to be in.

		Predicting,			// We are predicting the activation of this ability
		Confirmed,			// We are not the authority, but the authority has confirmed this activation
	};
}

// ----------------------------------------------------

USTRUCT(BlueprintType)
struct FGameplayAbilitySpecHandle
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilitySpecHandle()
	: Handle(INDEX_NONE)
	{

	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	void GenerateNewHandle()
	{
		static int32 GHandle = 1;
		Handle = GHandle++;
	}

	bool operator==(const FGameplayAbilitySpecHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FGameplayAbilitySpecHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FGameplayAbilitySpecHandle& SpecHandle)
	{
		return ::GetTypeHash(SpecHandle.Handle);
	}

private:

	UPROPERTY()
	int32 Handle;
};

/**
 *	FGameplayAbilityActorInfo
 *
 *	Cached data associated with an Actor using an Ability.
 *		-Initialized from an AActor* in InitFromActor
 *		-Abilities use this to know what to actor upon. E.g., instead of being coupled to a specific actor class.
 *		-These are generally passed around as pointers to support polymorphism.
 *		-Projects can override UAbilitySystemGlobals::AllocAbilityActorInfo to override the default struct type that is created.
 *
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityActorInfo
{
	GENERATED_USTRUCT_BODY()

	virtual ~FGameplayAbilityActorInfo() {}

	/** The actor that owns the abilities, shouldn't be null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<AActor>	OwnerActor;

	/** The physical representation of the owner, used for targeting and animation. This will often be null! */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<AActor>	AvatarActor;

	/** PlayerController associated with the owning actor. This will often be null! */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<APlayerController>	PlayerController;

	/** Ability System component associated with the owner actor, shouldn't be null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<UAbilitySystemComponent>	AbilitySystemComponent;

	/** Anim instance of the avatar actor. Often null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<UAnimInstance>	AnimInstance;

	/** Movement component of the avatar actor. Often null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<UMovementComponent>	MovementComponent;

	/** Returns true if this actor is locally controlled. Only true for players on the client that owns them */
	bool IsLocallyControlled() const;

	/** Returns true if the owning actor has net authority */
	bool IsNetAuthority() const;

	/** Initializes the info from an owning actor. Will set both owner and avatar */
	virtual void InitFromActor(AActor *OwnerActor, AActor *AvatarActor, UAbilitySystemComponent* InAbilitySystemComponent);

	/** Sets a new avatar actor, keeps same owner and ability system component */
	virtual void SetAvatarActor(AActor *AvatarActor);

	/** Clears out any actor info, both owner and avatar */
	virtual void ClearActorInfo();
};

/**
 *	FGameplayAbilityActivationInfo
 *
 *	Data tied to a specific activation of an ability.
 *		-Tell us whether we are the authority, if we are predicting, confirmed, etc.
 *		-Holds current and previous PredictionKey
 *		-Generally not meant to be subclassed in projects.
 *		-Passed around by value since the struct is small.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityActivationInfo
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilityActivationInfo()
		: ActivationMode(EGameplayAbilityActivationMode::Authority)
	{

	}

	FGameplayAbilityActivationInfo(AActor* InActor, FPredictionKey InPredictionKey)
		: PredictionKey(InPredictionKey)
	{
		// On Init, we are either Authority or NonAuthority. We haven't been given a PredictionKey and we haven't been confirmed.
		// NonAuthority essentially means 'I'm not sure what how I'm going to do this yet'.
		ActivationMode = (InActor->Role == ROLE_Authority ? EGameplayAbilityActivationMode::Authority : EGameplayAbilityActivationMode::NonAuthority);
	}

	FGameplayAbilityActivationInfo(EGameplayAbilityActivationMode::Type InType, FPredictionKey InPredictionKey = FPredictionKey())
		: ActivationMode(InType)
		, PredictionKey(InPredictionKey)
	{
	}

	void GenerateNewPredictionKey() const;

	void SetActivationConfirmed();

	void SetPredictionStale();

	FPredictionKey GetPredictionKeyForNewAction() const
	{
		return PredictionKey.IsValidForMorePrediction() ? PredictionKey : FPredictionKey();
	}

	FPredictionKey GetPredictionKey() const
	{
		return PredictionKey;
	}

	void SetPredictionKey(FPredictionKey NewKey) const
	{
		PredictionKey = NewKey;
	}

	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	mutable TEnumAsByte<EGameplayAbilityActivationMode::Type>	ActivationMode;

private:

	UPROPERTY()
	mutable FPredictionKey	PredictionKey;
	
};

// ----------------------------------------------------

USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayAbilitySpec
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilitySpec()
	: Ability(nullptr), Level(1), InputID(INDEX_NONE), SourceObject(nullptr), InputPressed(false), ActiveCount(0)
	{
		
	}

	FGameplayAbilitySpec(UGameplayAbility* InAbility, int32 InLevel=1, int32 InInputID=INDEX_NONE, UObject* InSourceObject=nullptr)
		: Ability(InAbility), Level(InLevel), InputID(InInputID), SourceObject(InSourceObject), InputPressed(false), ActiveCount(0)
	{
		Handle.GenerateNewHandle();
	}

	/** Handle for outside sources to refer to this spec by */
	UPROPERTY()
	FGameplayAbilitySpecHandle Handle;
	
	/** Ability of the spec (either CDO or instanced) */
	UPROPERTY()
	UGameplayAbility* Ability;
	
	/** Level of Ability */
	UPROPERTY()
	int32	Level;

	/** InputID, if bound */
	UPROPERTY()
	int32	InputID;

	/** Object this ability was created from, can be an actor or static object. Useful to bind an ability to a gameplay object */
	UPROPERTY()
	UObject* SourceObject;

	/** Is input currently pressed. Set to false when input is released */
	UPROPERTY(NotReplicated)
	bool	InputPressed;

	/** A count of the number of times this ability has been activated minus the number of times it has been ended. For instanced abilities this will be the number of currently active instances. Can't replicate until prediction accurately handles this.*/
	UPROPERTY(NotReplicated)
	uint8	ActiveCount;

	/** Activation state of this ability. This is not replicated since it needs to be overwritten locally on clients during prediction. */
	UPROPERTY(NotReplicated)
	FGameplayAbilityActivationInfo	ActivationInfo;

	/** Non replicating instances of this ability. */
	UPROPERTY(NotReplicated)
	TArray<UGameplayAbility*> NonReplicatedInstances;

	/** Replicated instances of this ability.. */
	UPROPERTY()
	TArray<UGameplayAbility*> ReplicatedInstances;

	TArray<UGameplayAbility*> GetAbilityInstances()
	{
		TArray<UGameplayAbility*> Abilities;
		Abilities.Append(ReplicatedInstances);
		Abilities.Append(NonReplicatedInstances);
		return Abilities;
	}

	bool IsActive() const;
};

// ----------------------------------------------------


/** Data about montages that is replicated to simulated clients */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayAbilityRepAnimMontage
{
	GENERATED_USTRUCT_BODY()

	/** AnimMontage ref */
	UPROPERTY()
	UAnimMontage* AnimMontage;

	/** Play Rate */
	UPROPERTY()
	float PlayRate;

	/** Montage position */
	UPROPERTY()
	float Position;

	/** NextSectionID */
	UPROPERTY()
	uint8 NextSectionID;

	/** Bit set when montage has been stopped. */
	UPROPERTY()
	uint8 IsStopped : 1;
	
	/** Bit flipped every time a new Montage is played. To trigger replication when the same montage is played again. */
	UPROPERTY()
	uint8 ForcePlayBit : 1;

	FGameplayAbilityRepAnimMontage()
		: AnimMontage(NULL),
		PlayRate(0.f),
		Position(0.f),
		NextSectionID(0),
		IsStopped(0),
		ForcePlayBit(0)
	{
	}

	UPROPERTY()
	FPredictionKey PredictionKey;
};

/** Data about montages that were played locally (all montages in case of server. predictive montages in case of client). Never replicated directly. */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayAbilityLocalAnimMontage
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilityLocalAnimMontage()
	: AnimMontage(nullptr), PlayBit(false), AnimatingAbility(nullptr)
	{
	}

	UPROPERTY()
	UAnimMontage* AnimMontage;

	UPROPERTY()
	bool PlayBit;

	UPROPERTY()
	FPredictionKey PredictionKey;

	/** The ability, if any, that instigated this montage */
	UPROPERTY()
	UGameplayAbility* AnimatingAbility;
};

// ----------------------------------------------------

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEventData
{
	GENERATED_USTRUCT_BODY()

	FGameplayEventData()
	: Instigator(NULL)
	, Target(NULL)
	{
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	AActor* Instigator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	AActor* Target;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	float Var1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	float Var2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	float Var3;

	FPredictionKey PredictionKey;
};

/** 
 *	Structure that tells AbilitySystemComponent what to bind to an InputComponent (see BindAbilityActivationToInputComponent) 
 *	
 */
struct FGameplayAbiliyInputBinds
{
	FGameplayAbiliyInputBinds(FString InConfirmTargetCommand, FString InCancelTargetCommand, FString InEnumName)
	: ConfirmTargetCommand(InConfirmTargetCommand)
	, CancelTargetCommand(InCancelTargetCommand)
	, EnumName(InEnumName)
	{ }

	/** Defines command string that will be bound to Confirm Targeting */
	FString ConfirmTargetCommand;

	/** Defines command string that will be bound to Cancel Targeting */
	FString CancelTargetCommand;

	/** Returns enum to use for ability bings. E.g., "Ability1"-"Ability8" input commands will be bound to ability activations inside the AbiltiySystemComponent */
	FString	EnumName;

	UEnum* GetBindEnum() { return FindObject<UEnum>(ANY_PACKAGE, *EnumName); }
};

USTRUCT()
struct GAMEPLAYABILITIES_API FAttributeDefaults
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "AttributeTest")
	TSubclassOf<UAttributeSet> Attributes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttributeTest")
	class UDataTable*	DefaultStartingTable;
};


/** Used for cleaning up predicted data on network clients */
DECLARE_MULTICAST_DELEGATE(FAbilitySystemComponentPredictionKeyClear);

/** Generic delegate for ability 'events'/notifies */
DECLARE_MULTICAST_DELEGATE_OneParam(FGenericAbilityDelegate, UGameplayAbility*);
