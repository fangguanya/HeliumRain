
#include "../../Flare.h"

#include "FlareSpacecraftWeaponsSystem.h"
#include "../FlareSpacecraft.h"

#define LOCTEXT_NAMESPACE "FlareSpacecraftWeaponsSystem"

/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareSpacecraftWeaponsSystem::UFlareSpacecraftWeaponsSystem(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	, Spacecraft(NULL)
{
}

UFlareSpacecraftWeaponsSystem::~UFlareSpacecraftWeaponsSystem()
{
	for (int32 GroupIndex = 0; GroupIndex < WeaponGroupList.Num(); GroupIndex++)
	{
		delete WeaponGroupList[GroupIndex];
	}
}

/*----------------------------------------------------
	Gameplay events
----------------------------------------------------*/

void UFlareSpacecraftWeaponsSystem::TickSystem(float DeltaSeconds)
{
	if (!ActiveWeaponGroup)
	{
		return;
	}

	switch (ActiveWeaponGroup->Type)
	{
		case EFlareWeaponGroupType::WG_GUN:
			for (int32 i = 0; i < ActiveWeaponGroup->Weapons.Num(); i++)
			{
				if (WantFire)
				{
					ActiveWeaponGroup->Weapons[i]->StartFire();
				}
				else
				{
					ActiveWeaponGroup->Weapons[i]->StopFire();
				}
			}

			break;
		case EFlareWeaponGroupType::WG_BOMB:
			if (Armed && WantFire)
			{
				Armed = false;
				int32 FireWeaponIndex = (ActiveWeaponGroup->LastFiredWeaponIndex+1) % ActiveWeaponGroup->Weapons.Num();
				ActiveWeaponGroup->Weapons[FireWeaponIndex]->StartFire();
				ActiveWeaponGroup->LastFiredWeaponIndex = FireWeaponIndex;
			}
			else if (!WantFire)
			{
				Armed = true;
			}

		case EFlareWeaponGroupType::WG_NONE:
		case EFlareWeaponGroupType::WG_TURRET:
		default:
			break;
	}


}

void UFlareSpacecraftWeaponsSystem::Initialize(AFlareSpacecraft* OwnerSpacecraft, FFlareSpacecraftSave* OwnerData)
{
	Spacecraft = OwnerSpacecraft;
	Components = Spacecraft->GetComponentsByClass(UFlareSpacecraftComponent::StaticClass());
	Description = Spacecraft->GetParent()->GetDescription();
	Data = OwnerData;
}

inline static bool ConstPredicate (const FFlareWeaponGroup& ip1, const FFlareWeaponGroup& ip2)
 {
	 return (ip1.Description->WeaponCharacteristics.Order < ip2.Description->WeaponCharacteristics.Order);
 }

void UFlareSpacecraftWeaponsSystem::Start()
{
	// Clear previous data
	WeaponList.Empty();
	WeaponDescriptionList.Empty();
	for (int32 GroupIndex = 0; GroupIndex < WeaponGroupList.Num(); GroupIndex++)
	{
		delete WeaponGroupList[GroupIndex];
	}
	WeaponGroupList.Empty();

	TArray<UActorComponent*> Weapons = Spacecraft->GetComponentsByClass(UFlareWeapon::StaticClass());
	for (int32 ComponentIndex = 0; ComponentIndex < Weapons.Num(); ComponentIndex++)
	{
		UFlareWeapon* Weapon = Cast<UFlareWeapon>(Weapons[ComponentIndex]);
		if(Weapon->GetDescription() == NULL)
		{
			FLOGV("ERROR: Weapon %s has no description", *Weapon->GetName());
			continue;
		}
		WeaponList.Add(Weapon);
		WeaponDescriptionList.Add(Weapon->GetDescription());
		int32 GroupIndex = GetGroupByWeaponIdentifer(Weapon->GetDescription()->Identifier);
		if (GroupIndex < 0)
		{
			// No existing group yet
			FFlareWeaponGroup* WeaponGroup = new FFlareWeaponGroup();

			WeaponGroup->Description = Weapon->GetDescription();

			if (WeaponGroup->Description->WeaponCharacteristics.TurretCharacteristics.IsTurret)
			{
				WeaponGroup->Type = EFlareWeaponGroupType::WG_TURRET;
			}
			else if (WeaponGroup->Description->WeaponCharacteristics.BombCharacteristics.IsBomb)
			{
				WeaponGroup->Type = EFlareWeaponGroupType::WG_BOMB;
			}
			else
			{
				WeaponGroup->Type = EFlareWeaponGroupType::WG_GUN;
			}
			WeaponGroup->LastFiredWeaponIndex = 0;
			WeaponGroup->Weapons.Add(Weapon);
			WeaponGroup->Target = NULL;

			Weapon->SetWeaponGroup(WeaponGroup);
			WeaponGroupList.Add(WeaponGroup);
		}
		else
		{
			FFlareWeaponGroup* WeaponGroup = WeaponGroupList[GroupIndex];
			WeaponGroup->Weapons.Add(Weapon);
			Weapon->SetWeaponGroup(WeaponGroup);
		}
	}

	// init last fired ammo with the one with the most ammo count
	for (int32 GroupIndex = 0; GroupIndex < WeaponGroupList.Num(); GroupIndex++)
	{

		FFlareWeaponGroup* WeaponGroup = WeaponGroupList[GroupIndex];

		//FLOGV("Group %d : component=%s", GroupIndex, *WeaponGroup->Description->Identifier.ToString())
		//FLOGV("Group %d : type=%d", GroupIndex, (int32)WeaponGroup->Type.GetValue());
		//FLOGV("Group %d : count=%d", GroupIndex, WeaponGroup->Weapons.Num());
		int MinAmmo = 0;
		int MinAmmoIndex = -1;

		for (int32 WeaponIndex = 0; WeaponIndex < WeaponGroup->Weapons.Num(); WeaponIndex++)
		{
			int32 CurrentAmmo = WeaponGroup->Weapons[WeaponIndex]->GetCurrentAmmo();
			if (MinAmmoIndex == -1 || CurrentAmmo < MinAmmo)
			{
				MinAmmo = CurrentAmmo;
				MinAmmoIndex = WeaponIndex;
			}
		}

		WeaponGroup->LastFiredWeaponIndex = MinAmmoIndex;
	}

	// Sort group
	WeaponGroupList.Sort(&ConstPredicate);

	// TODO save
	ActiveWeaponGroupIndex = -1;
	ActiveWeaponGroup = NULL;
	LastActiveWeaponGroupIndex = 0;
	WantFire = false;
	StopAllWeapons();
}

void UFlareSpacecraftWeaponsSystem::SetActiveWeaponTarget(AFlareSpacecraft* Target)
{
	if(ActiveWeaponGroup)
	{
		ActiveWeaponGroup->Target = Target;
	}
}

AFlareSpacecraft* UFlareSpacecraftWeaponsSystem::GetActiveWeaponTarget()
{
	if(ActiveWeaponGroup)
	{
		return ActiveWeaponGroup->Target;
	}
	return NULL;
}

void UFlareSpacecraftWeaponsSystem::StopAllWeapons()
{
	for (int32 WeaponIndex = 0; WeaponIndex < WeaponList.Num(); WeaponIndex++)
	{
		WeaponList[WeaponIndex]->StopFire();
	}
}



void UFlareSpacecraftWeaponsSystem::StartFire()
{
		WantFire = true;
}

void UFlareSpacecraftWeaponsSystem::StopFire()
{
		WantFire = false;
}

void UFlareSpacecraftWeaponsSystem::ActivateWeaponGroup(int32 Index)
{

	if (Index >= 0 && Index < WeaponGroupList.Num() && ActiveWeaponGroupIndex != Index)
	{
		StopAllWeapons();
		ActiveWeaponGroupIndex = Index;
		ActiveWeaponGroup = WeaponGroupList[Index];
		LastActiveWeaponGroupIndex = ActiveWeaponGroupIndex;
		Armed = false;
	}
}

void UFlareSpacecraftWeaponsSystem::ActivateWeapons(bool Activate)
{
	if (Activate)
	{
		ActivateWeapons();
	}
	else
	{
		DeactivateWeapons();
	}
}

void UFlareSpacecraftWeaponsSystem::ActivateWeapons()
{
	ActivateWeaponGroup(LastActiveWeaponGroupIndex);
}

void UFlareSpacecraftWeaponsSystem::DeactivateWeapons()
{
	StopFire();
	StopAllWeapons();
	 ActiveWeaponGroup = NULL;
	 ActiveWeaponGroupIndex = -1;
}

void UFlareSpacecraftWeaponsSystem::ToogleWeaponActivation()
{
	if (ActiveWeaponGroupIndex < 0)
	{
		ActivateWeapons();
	}
	else
	{
		DeactivateWeapons();
	}
}

int32 UFlareSpacecraftWeaponsSystem::GetGroupByWeaponIdentifer(FName Identifier) const
{
	for (int32 GroupIndex = 0; GroupIndex < WeaponGroupList.Num(); GroupIndex++)
	{
		if (WeaponGroupList[GroupIndex]->Description->Identifier == Identifier)
		{
			return GroupIndex;
		}
	}
	return -1;
}

EFlareWeaponGroupType::Type UFlareSpacecraftWeaponsSystem::GetActiveWeaponType() const
{
	if (ActiveWeaponGroup)
	{
		return ActiveWeaponGroup->Type;
	}
	return EFlareWeaponGroupType::WG_NONE;
}

bool UFlareSpacecraftWeaponsSystem::HasUsableWeaponType(EFlareWeaponGroupType::Type Type) const
{
	for (int32 GroupIndex = 0; GroupIndex < WeaponGroupList.Num(); GroupIndex++)
	{
		if (WeaponGroupList[GroupIndex]->Type == Type && Spacecraft->GetDamageSystem()->GetWeaponGroupHealth(GroupIndex, true))
		{
			return true;
		}
	}
	return false;
}

void UFlareSpacecraftWeaponsSystem::GetTargetPreference(float* IsSmall, float* IsLarge, float* IsUncontrollable, float* IsNotUncontrollable, float* IsStation, float* IsHarpooned)
{
	float LargePool = 0;
	float SmallPool = 0;
	float StationPool = 0;
	float UncontrollablePool = 0;
	float NotUncontrollablePool = 0;
	float HarpoonedPool = 0;


	for (int32 GroupIndex = 0; GroupIndex < WeaponGroupList.Num(); GroupIndex++)
	{
		EFlareShellDamageType::Type DamageType = WeaponGroupList[GroupIndex]->Description->WeaponCharacteristics.DamageType;

		if (Spacecraft->GetDamageSystem()->GetWeaponGroupHealth(GroupIndex, true) <= 0)
		{
			continue;
		}

		if (WeaponGroupList[GroupIndex]->Type == EFlareWeaponGroupType::WG_BOMB)
		{
			if(DamageType == EFlareShellDamageType::HEAT)
			{
				LargePool += 1.0;
				StationPool += 0.1;
				NotUncontrollablePool += 1.0;
			}
			else if(DamageType == EFlareShellDamageType::LightSalvage)
			{
				SmallPool += 1.0;
				UncontrollablePool += 1.0;

			}
			else if(DamageType == EFlareShellDamageType::HeavySalvage)
			{
				LargePool += 1.0;
				UncontrollablePool += 1.0;
			}
		}
		else
		{
			if (DamageType == EFlareShellDamageType::HEAT)
			{
				LargePool += 1.0;
				SmallPool += 0.1;
				StationPool = 0.1;
				NotUncontrollablePool += 1.0;
				HarpoonedPool += 1.0;
			}
			else
			{
				SmallPool += 1.0;
				NotUncontrollablePool += 1.0;
				HarpoonedPool += 1.0;
			}
		}
	}

	if(LargePool > 0 || SmallPool > 0)
	{
		FVector2D PoolVector = FVector2D(LargePool, SmallPool);
		PoolVector.Normalize();
		LargePool = PoolVector.X;
		SmallPool = PoolVector.Y;
	}

	if(NotUncontrollablePool > 0 || UncontrollablePool > 0)
	{
		FVector2D PoolVector = FVector2D(NotUncontrollablePool, UncontrollablePool);
		PoolVector.Normalize();
		NotUncontrollablePool = PoolVector.X;
		UncontrollablePool = PoolVector.Y;
	}

	StationPool = FMath::Clamp(StationPool, 0.f, 0.1f);
	HarpoonedPool = FMath::Clamp(HarpoonedPool, 0.f, 0.1f);

	*IsLarge  = LargePool;
	*IsSmall  = SmallPool;
	*IsNotUncontrollable  = NotUncontrollablePool;
	*IsUncontrollable  = UncontrollablePool;
	*IsStation = StationPool;
	*IsHarpooned = HarpoonedPool;
}

int32 UFlareSpacecraftWeaponsSystem::FindBestWeaponGroup(AFlareSpacecraft* Target)
{
	int32 BestWeaponGroup = -1;
	float BestScore = 0;

	if (!Target) {
		return -1;
	}

	bool LargeTarget = (Target->GetSize() == EFlarePartSize::L);
	bool SmallTarget = (Target->GetSize() == EFlarePartSize::S);
	bool UncontrollableTarget = Target->GetParent()->GetDamageSystem()->IsUncontrollable();

	for (int32 GroupIndex = 0; GroupIndex < WeaponGroupList.Num(); GroupIndex++)
	{
		float Score = Spacecraft->GetDamageSystem()->GetWeaponGroupHealth(GroupIndex, true);


		EFlareShellDamageType::Type DamageType = WeaponGroupList[GroupIndex]->Description->WeaponCharacteristics.DamageType;


		if (WeaponGroupList[GroupIndex]->Type == EFlareWeaponGroupType::WG_BOMB)
		{
			if(DamageType == EFlareShellDamageType::HEAT)
			{
				Score *= (LargeTarget ? 1.f : 0.f);
				Score *= (UncontrollableTarget ? 0.f : 1.f);
			}
			else if(DamageType == EFlareShellDamageType::LightSalvage)
			{
				Score *= (SmallTarget ? 1.f : 0.f);
				Score *= (UncontrollableTarget ? 1.f : 0.f);
				Score *= (Target->GetParent()->IsHarpooned() ? 0.f: 1.f);
			}
			else if(DamageType == EFlareShellDamageType::HeavySalvage)
			{
				Score *= (LargeTarget ? 1.f : 0.f);
				Score *= (UncontrollableTarget ? 1.f : 0.f);
				Score *= (Target->GetParent()->IsHarpooned() ? 0.f: 1.f);
			}
		}
		else
		{
			if (DamageType == EFlareShellDamageType::HEAT)
			{
				Score *= (LargeTarget ? 1.f : 0.1f);
				Score *= (UncontrollableTarget ? 0.f : 1.f);
			}
			else
			{
				Score *= (SmallTarget ? 1.f : 0.f);
				Score *= (UncontrollableTarget ? 0.f : 1.f);
			}
		}

		if (Score > 0 && Score > BestScore)
		{
			BestWeaponGroup = GroupIndex;
			BestScore = Score;
		}
	}

	return BestWeaponGroup;
}

#undef LOCTEXT_NAMESPACE
