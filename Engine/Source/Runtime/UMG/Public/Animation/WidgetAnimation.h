// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneAnimation.h"
#include "WidgetAnimationBinding.h"
#include "WidgetAnimation.generated.h"


class UMovieScene;
class UUserWidget;


UCLASS(BlueprintType, MinimalAPI)
class UWidgetAnimation
	: public UObject
	, public IMovieSceneAnimation
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITOR
	/**
	 * Get a placeholder animation.
	 *
	 * @return Placeholder animation.
	 */
	static UMG_API UWidgetAnimation* GetNullAnimation();
#endif

	/**
	 * Get the start time of this animation.
	 *
	 * @return Start time in seconds.
	 * @see GetEndTime
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetStartTime() const;

	/**
	 * Get the end time of this animation.
	 *
	 * @return End time in seconds.
	 * @see GetStartTime
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetEndTime() const;

	/**
	 * Initialize the widge editor with a new user widget.
	 *
	 * @param InPreviewWidget The user widget to preview.
	 */
	UMG_API void Initialize(UUserWidget* InPreviewWidget);

public:

	// IMovieSceneAnimation interface

	virtual bool AllowsSpawnableObjects() const override;
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject) override;
	virtual bool CanPossessObject(UObject& Object) const override;
	virtual void DestroyAllSpawnedObjects(UMovieScene& MovieScene) override { }
	virtual UObject* FindObject(const FGuid& ObjectId) const override;
	virtual FGuid FindObjectId(UObject& Object) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void SpawnOrDestroyObjects(UMovieScene* MovieScene, bool DestroyAll) override { }
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;

#if WITH_EDITOR
	virtual bool TryGetObjectDisplayName(const FGuid& ObjectId, FText& OutDisplayName) const override;
#endif

public:

	/** Pointer to the movie scene that controls this animation. */
	UPROPERTY()
	UMovieScene* MovieScene;

	UPROPERTY()
	TArray<FWidgetAnimationBinding> AnimationBindings;

private:

	/** The current preview widget, if any. */
	TWeakObjectPtr<UUserWidget> PreviewWidget;

	/** Mapping of preview objects to sequencer GUIDs */
	TMultiMap<FGuid, TWeakObjectPtr<UObject>> IdToPreviewObjects;
	TMultiMap<FGuid, TWeakObjectPtr<UObject>> IdToSlotContentPreviewObjects;
	TMap<TWeakObjectPtr<UObject>, FGuid> PreviewObjectToIds;
	TMap<TWeakObjectPtr<UObject>, FGuid> SlotContentPreviewObjectToIds;
};
