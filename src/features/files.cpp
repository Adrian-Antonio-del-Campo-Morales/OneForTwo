/*
 * OneInTwo — Ofertas y transferencia de archivos.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Crear ofertas limitadas a archivos elegidos o copiados por el usuario; asignar identificadores opacos.
 * - Gestionar aceptación, destino, manifiesto acotado, envío por bloques, progreso, cancelación y errores.
 * - Resolver rutas autorizadas, colisiones y tipos de archivo; impedir escapes por enlaces o componentes manipulados.
 * - Escribir temporales, comprobar tamaño e integridad y publicar resultados sin sobrescribir silenciosamente.
 * - Coordinar el adaptador de pegado nativo y la vida de la caché, si esa alternativa resulta elegida.
 * - Cancelar solicitudes obsoletas al cambiar la oferta, los permisos o la sesión, definiendo la vida de transferencias ya aceptadas.
 * - No borrar originales, conceder acceso arbitrario a rutas ni dar por resuelto el pegado nativo con un botón de envío.
 */

