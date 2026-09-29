idf_component_register(
    SRCS
        "AsyncTimer.cpp"
        "BlinkAssistant.cpp"
        "Eye.cpp"
        "EyeBlink.cpp"
        "EyeTransformation.cpp"
        "EyeTransition.cpp"
        "EyeVariation.cpp"
        "Face.cpp"
        "FaceBehavior.cpp"
        "FaceExpression.cpp"
        "LookAssistant.cpp"
    INCLUDE_DIRS "."
    REQUIRES u8g2
)
