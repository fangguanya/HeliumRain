#pragma once

#include "../Spacecrafts/FlareSpacecraftInterface.h"
#include "FlareSpacecraftCatalogEntry.generated.h"


UCLASS()
class HELIUMRAIN_API UFlareSpacecraftCatalogEntry : public UDataAsset
{
	GENERATED_UCLASS_BODY()

public:

	/*----------------------------------------------------
		Public data
	----------------------------------------------------*/
	
	/** Ship data */
	UPROPERTY(EditAnywhere, Category = Content)
	FFlareSpacecraftDescription Data;

};