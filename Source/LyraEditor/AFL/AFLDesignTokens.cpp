// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDesignTokens.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	/** sRGB hex -> linear. UMG colours are LINEAR; feeding the page's sRGB hex straight in renders washed out. */
	FLinearColor SrgbHexToLinear(uint8 R, uint8 G, uint8 B, uint8 A)
	{
		return FLinearColor(FColor(R, G, B, A));   // FColor->FLinearColor applies the sRGB curve
	}

	bool ParseHexColor(const FString& InRaw, FLinearColor& Out)
	{
		FString Hex = InRaw;
		Hex.TrimStartAndEndInline();
		if (!Hex.StartsWith(TEXT("#")))
		{
			return false;
		}
		Hex.RightChopInline(1);

		auto Nibble = [](TCHAR C, int32& OutVal) -> bool
		{
			if (C >= '0' && C <= '9') { OutVal = C - '0'; return true; }
			if (C >= 'a' && C <= 'f') { OutVal = 10 + (C - 'a'); return true; }
			if (C >= 'A' && C <= 'F') { OutVal = 10 + (C - 'A'); return true; }
			return false;
		};
		auto Byte = [&Nibble](const FString& S, int32 Index, uint8& OutByte) -> bool
		{
			int32 Hi = 0, Lo = 0;
			if (!Nibble(S[Index], Hi) || !Nibble(S[Index + 1], Lo)) { return false; }
			OutByte = static_cast<uint8>((Hi << 4) | Lo);
			return true;
		};

		uint8 R = 0, G = 0, B = 0, A = 255;
		if (Hex.Len() == 3)
		{
			// #RGB shorthand -- each nibble doubled, per CSS.
			int32 r = 0, g = 0, b = 0;
			if (!Nibble(Hex[0], r) || !Nibble(Hex[1], g) || !Nibble(Hex[2], b)) { return false; }
			R = static_cast<uint8>(r * 17); G = static_cast<uint8>(g * 17); B = static_cast<uint8>(b * 17);
		}
		else if (Hex.Len() == 6 || Hex.Len() == 8)
		{
			if (!Byte(Hex, 0, R) || !Byte(Hex, 2, G) || !Byte(Hex, 4, B)) { return false; }
			if (Hex.Len() == 8 && !Byte(Hex, 6, A)) { return false; }
		}
		else
		{
			return false;
		}

		Out = SrgbHexToLinear(R, G, B, A);
		return true;
	}

	bool ParseRgbaColor(const FString& InRaw, FLinearColor& Out)
	{
		FString S = InRaw;
		S.TrimStartAndEndInline();
		const bool bIsRgba = S.StartsWith(TEXT("rgba("));
		const bool bIsRgb = S.StartsWith(TEXT("rgb("));
		if (!bIsRgba && !bIsRgb)
		{
			return false;
		}
		const int32 Open = S.Find(TEXT("("));
		const int32 Close = S.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Open == INDEX_NONE || Close == INDEX_NONE || Close <= Open)
		{
			return false;
		}

		TArray<FString> Parts;
		S.Mid(Open + 1, Close - Open - 1).ParseIntoArray(Parts, TEXT(","), /*bCullEmpty=*/true);
		if (Parts.Num() < 3)
		{
			return false;
		}
		for (FString& P : Parts) { P.TrimStartAndEndInline(); }

		const uint8 R = static_cast<uint8>(FMath::Clamp(FCString::Atoi(*Parts[0]), 0, 255));
		const uint8 G = static_cast<uint8>(FMath::Clamp(FCString::Atoi(*Parts[1]), 0, 255));
		const uint8 B = static_cast<uint8>(FMath::Clamp(FCString::Atoi(*Parts[2]), 0, 255));
		const float Alpha = (Parts.Num() >= 4) ? FMath::Clamp(FCString::Atof(*Parts[3]), 0.f, 1.f) : 1.f;

		// Colour channels go through the sRGB curve; ALPHA DOES NOT -- it is coverage, not light. Running
		// alpha through the curve is the classic mistake here and makes every glass panel too opaque.
		Out = SrgbHexToLinear(R, G, B, 255);
		Out.A = Alpha;
		return true;
	}
}

bool FAFLDesignTokens::TryGetString(const FString& TokenName, FString& OutValue) const
{
	if (const FString* Found = Raw.Find(TokenName))
	{
		OutValue = *Found;
		return true;
	}
	return false;
}

bool FAFLDesignTokens::TryGetColor(const FString& TokenName, FLinearColor& OutColor) const
{
	FString Value;
	if (!TryGetString(TokenName, Value))
	{
		return false;
	}
	return ParseHexColor(Value, OutColor) || ParseRgbaColor(Value, OutColor);
}

bool FAFLDesignTokens::TryGetScalar(const FString& TokenName, float& OutValue) const
{
	FString Value;
	if (!TryGetString(TokenName, Value))
	{
		return false;
	}
	Value.TrimStartAndEndInline();
	Value.RemoveFromEnd(TEXT("px"));
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty() || !(FChar::IsDigit(Value[0]) || Value[0] == '.' || Value[0] == '-'))
	{
		return false;   // a gradient, a font stack, a cubic-bezier -- not a scalar
	}
	OutValue = FCString::Atof(*Value);
	return true;
}

bool FAFLDesignTokens::TryGetColorAny(const TArray<FString>& TokenNames, FLinearColor& OutColor) const
{
	for (const FString& Name : TokenNames)
	{
		if (TryGetColor(Name, OutColor))
		{
			return true;
		}
	}
	return false;
}

bool FAFLDesignTokens::TryGetScalarAny(const TArray<FString>& TokenNames, float& OutValue) const
{
	for (const FString& Name : TokenNames)
	{
		if (TryGetScalar(Name, OutValue))
		{
			return true;
		}
	}
	return false;
}

bool FAFLDesignTokenParser::ParseFile(const FString& AbsoluteHtmlPath, FAFLDesignTokens& Out, FString& OutError)
{
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *AbsoluteHtmlPath))
	{
		OutError = FString::Printf(TEXT("could not read '%s'"), *AbsoluteHtmlPath);
		return false;
	}
	Out.SourceFile = AbsoluteHtmlPath;
	return ParseString(Contents, Out, OutError);
}

bool FAFLDesignTokenParser::ParseString(const FString& Css, FAFLDesignTokens& Out, FString& OutError)
{
	// Scan for `--name:value;` across the whole document rather than isolating :root{}. The pages declare
	// tokens in :root and again under theme selectors (`:root[data-theme="dark"]`), and a later declaration
	// legitimately overrides an earlier one -- which is exactly what a last-write-wins map reproduces. The
	// alternative (parse only the first :root) would silently take the light-theme values from a page whose
	// shipping surface is hard-pinned dark.
	int32 Cursor = 0;
	int32 Count = 0;
	while (true)
	{
		const int32 Start = Css.Find(TEXT("--"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
		if (Start == INDEX_NONE)
		{
			break;
		}

		const int32 Colon = Css.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		const int32 Semi = Css.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (Colon == INDEX_NONE || Semi == INDEX_NONE || Colon > Semi)
		{
			Cursor = Start + 2;
			continue;   // a `--` inside a comment or a calc(); not a declaration
		}

		FString Name = Css.Mid(Start, Colon - Start);
		Name.TrimStartAndEndInline();

		// A declaration name has no whitespace or braces. Anything else is prose containing "--".
		if (Name.Contains(TEXT(" ")) || Name.Contains(TEXT("{")) || Name.Contains(TEXT("\n")) || Name.Len() < 3)
		{
			Cursor = Start + 2;
			continue;
		}

		FString Value = Css.Mid(Colon + 1, Semi - Colon - 1);
		Value.TrimStartAndEndInline();

		if (!Value.IsEmpty())
		{
			Out.Raw.Add(Name, Value);   // last write wins -- see the theme-override note above
			++Count;
		}
		Cursor = Semi + 1;
	}

	if (Count == 0)
	{
		OutError = TEXT("no CSS custom properties found -- is this a design page?");
		return false;
	}
	return true;
}
