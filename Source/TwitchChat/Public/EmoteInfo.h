#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture.h" 
#include "EmoteInfo.generated.h"


USTRUCT(BlueprintType)
struct FEmoteInfo : public FTableRowBase
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote Info") FString                   Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote Info") FString                   Code;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote Info") TSoftObjectPtr<UTexture>  Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote Info") FVector2D                 Size = FVector2D(18, 18);
};
