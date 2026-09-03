// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "AFLChatTypes.h" // EAFLChatChannel, FAFLChatMessage (AFLNetTypes)

#include "AFLW_Chat.generated.h"

class UScrollBox;
class UVerticalBox;
class UEditableTextBox;
class UTextBlock;

/**
 * UAFLW_Chat -- the COMMS-2 chat surface (BM-COMMS2-01/02), built PURE-C++ in RebuildWidget the same way
 * UAFLW_SystemMenu is (no WBP asset, no editor/bridge dependency) and styled from the ratified IRONICS tokens.
 *
 * It is the single UI touchpoint to the COMMS-1 spine: subscribes to UAFLChatSubsystem::OnChatMessage (raw C++
 * multicast -> a C++ widget must bridge it; a WBP could not), backfills GetHistory() on open, and sends via
 * UAFLChatSubsystem::Send. Channel colour code: Say white / Team #1E5AFF / Whisper #A855F7 (purple) / System
 * #E0A93A (amber). The client-synthesized drop-echo line (bLocalEphemeral, R3) renders extra-dim and is never
 * re-sendable. Menu-mode input (GetDesiredInputConfig) makes Esc close chat BEFORE the System-Menu preprocessor
 * fires (the preprocessor yields when a CommonUI widget owns focus).
 *
 * Opened via the `afl.Chat.Open` console command (autonomous mount -- no editor-authored input asset yet); a
 * ratified Enter/IMC binding is a later increment.
 */
UCLASS()
class AFLCOMBAT_API UAFLW_Chat : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_Chat();

	/** Push the chat panel onto UI.Layer.Menu for the local player. */
	static UAFLW_Chat* Open(const UObject* WorldContext);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

private:
	/** Inbound from the spine (real messages + the client-synthesized drop echo). */
	void HandleChatMessage(const FAFLChatMessage& Message);
	/** Build one message row and append it to the list (trimmed to MaxRows). */
	void AppendRow(const FAFLChatMessage& Message);
	/** Enter in the compose box -> send. UFUNCTION: bound to UEditableTextBox::OnTextCommitted via AddDynamic. */
	UFUNCTION()
	void OnComposeCommitted(const FText& Text, ETextCommit::Type CommitType);
	/** Keystroke -> live 0/256 counter (amber near the cap). UFUNCTION: OnTextChanged via AddDynamic. */
	UFUNCTION()
	void OnComposeChanged(const FText& Text);
	/** Tab cycles the sendable channels (Say -> Team -> Whisper). System/Party are never sendable. */
	void CycleChannel();
	void RefreshChannelChip();
	/** Read the compose box, resolve a whisper target if any, hand the body to the spine. */
	void SendCurrent();

	// --- widgets constructed in RebuildWidget ---
	UPROPERTY(Transient) TObjectPtr<UScrollBox>       MessageScroll;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox>     MessageBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ComposeInput;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       ChannelChip;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       CharCounter;

	EAFLChatChannel CurrentChannel = EAFLChatChannel::Say;
	FDelegateHandle ChatHandle;

	static constexpr int32 MaxRows = 60;
};
