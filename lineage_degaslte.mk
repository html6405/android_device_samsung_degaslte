# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

$(call inherit-product, device/samsung/degaslte/full_degaslte.mk)

PRODUCT_NAME := lineage_degaslte
