// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLLaserVisualStatics.h"

#include "AFLLaserVisualProvider.h"
#include "NiagaraComponent.h"
#include "UObject/UnrealType.h"

void UAFLLaserVisualStatics::DriveLaserTint(UNiagaraComponent* NiagaraComp, const UObject* Provider, FName ColorParam)
{
	if (!NiagaraComp || !Provider)
	{
		return;
	}

	// Keep the interface ONLY as the "is-a-laser-provider" marker.
	if (!Provider->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
	{
		return;
	}

	// Read the per-weapon tint by REFLECTION (LaserTintColor) instead of Execute_GetBeamColor: that
	// BlueprintNativeEvent's bridge-wired BP override never dispatches at runtime (Execute_ returns the
	// C++ default (0,0,0,0)), so the cue would always see no tint. A>0 = a real tint -> drive User.Color;
	// A<=0 (unset / no property) = leave the NS at its authored colour, so existing weapons are unchanged.
	const FLinearColor TintColor = ReadLaserTint(Provider);
	if (TintColor.A > 0.f)
	{
		NiagaraComp->SetVariableLinearColor(ColorParam, TintColor);
	}
}

FLinearColor UAFLLaserVisualStatics::ReadLaserTint(const UObject* Provider)
{
	if (!Provider)
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f); // sentinel: A<=0 -> caller keeps its default
	}

	// Reflection read of the BP var LaserTintColor (the value the cosmetic resolver/cheat WRITES by the
	// identical reflection -- proven at HOP3). This is the dispatch-proof replacement for the broken
	// Execute_GetBeamColor; the interface remains only as the ImplementsInterface marker.
	const FProperty* Prop = Provider->GetClass()->FindPropertyByName(FName(TEXT("LaserTintColor")));
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			return *StructProp->ContainerPtrToValuePtr<FLinearColor>(Provider);
		}
	}
	return FLinearColor(0.f, 0.f, 0.f, 0.f); // no LaserTintColor property -> sentinel -> caller keeps default
}
