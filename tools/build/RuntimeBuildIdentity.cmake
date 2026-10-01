function(tetrisphere_receipt_identity output_variable receipt_path)
    file(READ "${receipt_path}" receipt_json)
    foreach(field IN ITEMS name commit source_identity lock_digest)
        string(JSON receipt_${field} ERROR_VARIABLE receipt_error GET
               "${receipt_json}" "${field}")
        if(receipt_error)
            message(FATAL_ERROR "dependency receipt lacks ${field}: ${receipt_error}")
        endif()
    endforeach()
    string(SHA256 canonical_identity
           "${receipt_name}:${receipt_commit}:${receipt_source_identity}:${receipt_lock_digest}")
    set(${output_variable} "${canonical_identity}" PARENT_SCOPE)
endfunction()

function(tetrisphere_bind_runtime_identity output_variable build_material runtime_source_identity)
    string(SHA256 bound_identity "${build_material}:runtime-source:${runtime_source_identity}")
    set(${output_variable} "${bound_identity}" PARENT_SCOPE)
endfunction()
