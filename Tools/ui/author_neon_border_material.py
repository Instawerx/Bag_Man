# Author M_AFL_NeonBorder -- the ROLLING ELECTRIC-NEON BORDER for the Landing sign-in card (Identity I-2,
# operator ask 2026-09-15: "extra points for rolling electric neon borders"). A UI-domain material: a bright
# segment sweeps around the card edge once every ~3 s with a soft #1E5AFF glow; reduced-motion = Speed 0.
#
# Run headless with the editor DOWN (builds are the operator's; this only writes one asset):
#   D:\UE5.6-source\Engine\Binaries\Win64\UnrealEditor-Cmd.exe C:\Dev\Bag_Man\Bag_Man.uproject
#     -run=pythonscript -script="C:\Dev\Bag_Man\Tools\ui\author_neon_border_material.py" -unattended -nop4 -nosplash
# Idempotent: an existing asset is left alone (delete it in the editor to re-author).
import unreal

PKG_PATH = "/Game/BagMan/UI/Materials"
NAME = "M_AFL_NeonBorder"
FULL = f"{PKG_PATH}/{NAME}"

HLSL = r"""
// UV in [0,1] over the card quad. The border ring is the last `Thickness` of UV space on every side.
float2 d = min(UV, 1.0 - UV);
float edge = min(d.x, d.y);
float ring = 1.0 - smoothstep(Thickness, Thickness * 1.7, edge);
// Angle around the centre, 0..1, then a head that sweeps with time and fades along its tail.
float2 p = UV - 0.5;
float a = atan2(p.y, p.x) / 6.28318530 + 0.5;
float sweep = frac(a - T * Speed);
float head = pow(saturate(1.0 - sweep / Band), 2.2);
// Resting glow keeps the border visible when the head is elsewhere (and when Speed is 0).
float glow = ring * (0.22 + 0.78 * head);
return float4(glow, head * ring, 0.0, glow);
"""

def main():
    if unreal.EditorAssetLibrary.does_asset_exist(FULL):
        unreal.log(f"[neon] {FULL} exists -- nothing authored")
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(NAME, PKG_PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    lib = unreal.MaterialEditingLibrary

    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, -200)
    t = lib.create_material_expression(mat, unreal.MaterialExpressionTime, -900, -80)
    speed = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 20)
    speed.set_editor_property("parameter_name", "Speed"); speed.set_editor_property("default_value", 0.32)
    band = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 120)
    band.set_editor_property("parameter_name", "Band"); band.set_editor_property("default_value", 0.30)
    thick = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 220)
    thick.set_editor_property("parameter_name", "Thickness"); thick.set_editor_property("default_value", 0.035)

    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, -560, 0)
    custom.set_editor_property("code", HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", "NeonSweep")
    inputs = []
    for n in ("UV", "T", "Speed", "Band", "Thickness"):
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    lib.connect_material_expressions(uv, "", custom, "UV")
    lib.connect_material_expressions(t, "", custom, "T")
    lib.connect_material_expressions(speed, "", custom, "Speed")
    lib.connect_material_expressions(band, "", custom, "Band")
    lib.connect_material_expressions(thick, "", custom, "Thickness")

    # Colour: brand accent #1E5AFF at rest, lifting toward #9FC0FF at the head. glow = R, head = G.
    base = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -560, 260)
    base.set_editor_property("parameter_name", "Color"); base.set_editor_property("default_value", unreal.LinearColor(0.118, 0.353, 1.0, 1.0))
    hot = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -560, 380)
    hot.set_editor_property("parameter_name", "HeadColor"); hot.set_editor_property("default_value", unreal.LinearColor(0.624, 0.753, 1.0, 1.0))
    mask_r = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, -40)
    mask_r.set_editor_property("r", True); mask_r.set_editor_property("g", False); mask_r.set_editor_property("b", False); mask_r.set_editor_property("a", False)
    mask_g = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, 60)
    mask_g.set_editor_property("r", False); mask_g.set_editor_property("g", True); mask_g.set_editor_property("b", False); mask_g.set_editor_property("a", False)
    lib.connect_material_expressions(custom, "", mask_r, "")
    lib.connect_material_expressions(custom, "", mask_g, "")
    lerp = lib.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -120, 200)
    lib.connect_material_expressions(base, "", lerp, "A")
    lib.connect_material_expressions(hot, "", lerp, "B")
    lib.connect_material_expressions(mask_g, "", lerp, "Alpha")
    intensity = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -120, 320)
    intensity.set_editor_property("parameter_name", "Intensity"); intensity.set_editor_property("default_value", 1.6)
    mul_color = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, 60, 120)
    lib.connect_material_expressions(lerp, "", mul_color, "A")
    lib.connect_material_expressions(mask_r, "", mul_color, "B")
    mul_int = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, 220, 120)
    lib.connect_material_expressions(mul_color, "", mul_int, "A")
    lib.connect_material_expressions(intensity, "", mul_int, "B")

    lib.connect_material_property(mul_int, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(mask_r, "", unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(FULL, only_if_is_dirty=False)
    unreal.log(f"[neon] authored + saved {FULL}")

main()
